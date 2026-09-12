#include "files.h"

#include <fcntl.h>
#include <limits.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log.h"

#define LOG_LABEL "files"

// on linux 5.6+ the kernel refuses to resolve outside the root itself.
// the portable path below stays as fallback and as the second layer.
#if defined(__linux__) && defined(__has_include)
#  if __has_include(<linux/openat2.h>)
#    include <linux/openat2.h>
#    define HAVE_OPENAT2 1
#  endif
#endif

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

static const char *file_media_type(const char *path) {
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

static file_result result_for_errno(int err) {
    switch (err) {
    case ENOENT:
    case ENOTDIR:       return FILE_NOT_FOUND;
    case EACCES:
    case EPERM:
    case ELOOP:
    case EXDEV:         return FILE_FORBIDDEN;
    case ENAMETOOLONG:  return FILE_BAD_TARGET;
    default:            return FILE_ERROR;
    }
}

#ifdef HAVE_OPENAT2
// -1 with errno set on a real failure, -2 when the kernel has no openat2
static int open_beneath(int root_fd, const char *relative) {
    static int unsupported = 0;

    if (unsupported) {
        return -2;
    }

    struct open_how how = {
        .flags   = O_RDONLY,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS,
    };

    int fd = (int)syscall(SYS_openat2, root_fd, relative, &how, sizeof(how));

    if (fd == -1 && (errno == ENOSYS || errno == EINVAL)) {
        unsupported = 1;
        return -2;
    }

    return fd;
}
#endif

// the portable second layer: normalize, resolve, then check the prefix
static int open_checked(const char *root, const char *normalized,
                        file_result *out_err) {
    char root_real[PATH_MAX];
    if (realpath(root, root_real) == NULL) {
        log_errno("realpath(root)");
        *out_err = FILE_ERROR;
        return -1;
    }

    char candidate[PATH_MAX * 2];
    snprintf(candidate, sizeof(candidate), "%s%s", root_real, normalized);

    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) == NULL) {
        *out_err = FILE_NOT_FOUND;
        return -1;
    }

    size_t root_len = strlen(root_real);
    if (strncmp(resolved, root_real, root_len) != 0 ||
        (resolved[root_len] != '/' && resolved[root_len] != '\0')) {
        log_error("target escaped the root: %s", normalized);
        *out_err = FILE_FORBIDDEN;
        return -1;
    }

    // open first and fstat the fd: nothing can be swapped in between
    int fd = open(resolved, O_RDONLY);
    if (fd == -1) {
        *out_err = result_for_errno(errno);
    }
    return fd;
}

static file_result read_from_fd(int fd, size_t size, const char *path,
                                file_content *out) {
    char *data = malloc(size + 1);
    if (data == NULL) {
        log_error("out of memory for %s", path);
        return FILE_ERROR;
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
            return FILE_ERROR;
        }

        if (n == 0) {
            break;
        }

        total += (size_t)n;
    }

    data[total] = '\0';

    out->data = data;
    out->len = total;
    out->media_type = file_media_type(path);
    return FILE_OK;
}

// opens `normalized` under `root` without letting it escape
static int open_target(const char *root, int root_fd, const char *normalized,
                       file_result *out_err) {
#ifdef HAVE_OPENAT2
    if (root_fd != -1) {
        const char *relative = (normalized[1] == '\0') ? "." : normalized + 1;
        int fd = open_beneath(root_fd, relative);

        if (fd >= 0) {
            return fd;
        }
        if (fd == -1) {
            *out_err = result_for_errno(errno);
            if (*out_err == FILE_FORBIDDEN) {
                log_error("target escaped the root: %s", normalized);
            }
            return -1;
        }
        // fd == -2: kernel is older, fall through
    }
#else
    (void)root_fd;
#endif

    return open_checked(root, normalized, out_err);
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

    int root_fd = open(root, O_RDONLY | O_DIRECTORY);

    file_result err = FILE_NOT_FOUND;
    int fd = open_target(root, root_fd, normalized, &err);
    if (fd == -1) {
        if (root_fd != -1) close(root_fd);
        return err;
    }

    struct stat info;
    if (fstat(fd, &info) == -1) {
        close(fd);
        if (root_fd != -1) close(root_fd);
        return FILE_ERROR;
    }

    char index[PATH_MAX];
    if (S_ISDIR(info.st_mode)) {
        int n = snprintf(index, sizeof(index), "%s%sindex.html", normalized,
                         normalized[strlen(normalized) - 1] == '/' ? "" : "/");
        close(fd);

        if (n < 0 || (size_t)n >= sizeof(index)) {
            if (root_fd != -1) close(root_fd);
            return FILE_BAD_TARGET;
        }

        fd = open_target(root, root_fd, index, &err);
        if (fd == -1) {
            if (root_fd != -1) close(root_fd);
            return err == FILE_NOT_FOUND ? FILE_FORBIDDEN : err;
        }

        if (fstat(fd, &info) == -1) {
            close(fd);
            if (root_fd != -1) close(root_fd);
            return FILE_ERROR;
        }

        snprintf(normalized, sizeof(normalized), "%s", index);
    }

    if (root_fd != -1) {
        close(root_fd);
    }

    if (!S_ISREG(info.st_mode)) {
        close(fd);
        return FILE_FORBIDDEN;
    }

    if (info.st_size > FILE_MAX_SIZE) {
        close(fd);
        return FILE_TOO_LARGE;
    }

    file_result r = read_from_fd(fd, (size_t)info.st_size, normalized, out);
    close(fd);
    return r;
}

void file_content_free(file_content *content) {
    free(content->data);
    content->data = NULL;
    content->len = 0;
}
