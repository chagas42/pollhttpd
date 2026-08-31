#include "files.h"

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log.h"

#define LOG_LABEL "files"

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int percent_decode(const char *src, char *out, size_t cap) {
    size_t len = 0;

    while (*src != '\0') {
        char c;

        if (*src == '%') {
            if (src[1] == '\0' || src[2] == '\0') {
                return -1;
            }
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);
            if (hi < 0 || lo < 0) {
                return -1;
            }
            c = (char)((hi << 4) | lo);
            src += 3;
        } else {
            c = *src;
            src += 1;
        }

        if (c == '\0' || (unsigned char)c < 0x20 || (unsigned char)c == 0x7f) {
            return -1;
        }
        if (len + 1 >= cap) {
            return -1;
        }

        out[len] = c;
        len += 1;
    }

    out[len] = '\0';
    return 0;
}

static int normalize_path(const char *path, char *out, size_t cap) {
    size_t len = 0;
    const char *p = path;

    out[0] = '\0';

    while (*p != '\0') {
        while (*p == '/') {
            p += 1;
        }
        if (*p == '\0') {
            break;
        }

        const char *segment = p;
        while (*p != '\0' && *p != '/') {
            p += 1;
        }
        size_t segment_len = (size_t)(p - segment);

        if (segment_len == 1 && segment[0] == '.') {
            continue;
        }

        if (segment_len == 2 && segment[0] == '.' && segment[1] == '.') {
            if (len == 0) {
                return -1;
            }
            while (len > 0 && out[len - 1] != '/') {
                len -= 1;
            }
            if (len > 0) {
                len -= 1;
            }
            out[len] = '\0';
            continue;
        }

        if (len + 1 + segment_len + 1 > cap) {
            return -1;
        }

        out[len] = '/';
        len += 1;
        memcpy(out + len, segment, segment_len);
        len += segment_len;
        out[len] = '\0';
    }

    if (len == 0) {
        if (cap < 2) {
            return -1;
        }
        out[0] = '/';
        out[1] = '\0';
    }

    return 0;
}

const char *file_media_type(const char *path) {
    const char *dot = strrchr(path, '.');

    if (dot == NULL) {
        return "application/octet-stream";
    }

    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".css") == 0)  return "text/css; charset=utf-8";
    if (strcmp(dot, ".js") == 0)   return "text/javascript; charset=utf-8";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".txt") == 0)  return "text/plain; charset=utf-8";
    if (strcmp(dot, ".svg") == 0)  return "image/svg+xml";
    if (strcmp(dot, ".png") == 0)  return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".ico") == 0)  return "image/x-icon";

    return "application/octet-stream";
}

static file_result read_whole_file(const char *path, size_t size, file_content *out) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return FILE_NOT_FOUND;
    }

    char *data = malloc(size + 1);
    if (data == NULL) {
        close(fd);
        log_error("out of memory for %s", path);
        return FILE_NOT_FOUND;
    }

    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, data + total, size - total);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            log_errno("read");
            free(data);
            close(fd);
            return FILE_NOT_FOUND;
        }

        if (n == 0) {
            break;
        }

        total += (size_t)n;
    }

    close(fd);
    data[total] = '\0';

    out->data = data;
    out->len = total;
    out->media_type = file_media_type(path);
    return FILE_OK;
}

file_result file_load(const char *root, const char *target, file_content *out) {
    out->data = NULL;
    out->len = 0;
    out->media_type = NULL;

    char without_query[PATH_MAX];
    size_t path_len = strcspn(target, "?#");
    if (path_len + 1 > sizeof(without_query)) {
        return FILE_BAD_TARGET;
    }
    memcpy(without_query, target, path_len);
    without_query[path_len] = '\0';

    char decoded[PATH_MAX];
    if (percent_decode(without_query, decoded, sizeof(decoded)) == -1) {
        return FILE_BAD_TARGET;
    }

    char normalized[PATH_MAX];
    if (normalize_path(decoded, normalized, sizeof(normalized)) == -1) {
        return FILE_FORBIDDEN;
    }

    char root_real[PATH_MAX];
    if (realpath(root, root_real) == NULL) {
        log_errno("realpath(root)");
        return FILE_NOT_FOUND;
    }

    char candidate[PATH_MAX * 2];
    snprintf(candidate, sizeof(candidate), "%s%s", root_real, normalized);

    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) == NULL) {
        return FILE_NOT_FOUND;
    }

    size_t root_len = strlen(root_real);
    if (strncmp(resolved, root_real, root_len) != 0 ||
        (resolved[root_len] != '/' && resolved[root_len] != '\0')) {
        log_error("target escaped the root: %s", target);
        return FILE_FORBIDDEN;
    }

    struct stat info;
    if (stat(resolved, &info) == -1) {
        return FILE_NOT_FOUND;
    }

    if (S_ISDIR(info.st_mode)) {
        char index[PATH_MAX * 2];
        snprintf(index, sizeof(index), "%s/index.html", resolved);

        if (stat(index, &info) == -1 || !S_ISREG(info.st_mode)) {
            return FILE_FORBIDDEN;
        }
        if (info.st_size > FILE_MAX_SIZE) {
            return FILE_TOO_LARGE;
        }
        return read_whole_file(index, (size_t)info.st_size, out);
    }

    if (!S_ISREG(info.st_mode)) {
        return FILE_FORBIDDEN;
    }
    if (info.st_size > FILE_MAX_SIZE) {
        return FILE_TOO_LARGE;
    }

    return read_whole_file(resolved, (size_t)info.st_size, out);
}

void file_content_free(file_content *content) {
    free(content->data);
    content->data = NULL;
    content->len = 0;
}
