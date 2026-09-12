#pragma once

#include <stddef.h>

#define FILE_MAX_SIZE (1024 * 1024)

typedef enum {
    FILE_OK,
    FILE_NOT_FOUND,
    FILE_FORBIDDEN,
    FILE_TOO_LARGE,
    FILE_BAD_TARGET,
    FILE_ERROR,          // ours, not the client's: answer 500, not 404
} file_result;

typedef struct {
    char       *data;
    size_t      len;
    const char *media_type;
} file_content;

file_result file_load(const char *root, const char *target, file_content *out);

void file_content_free(file_content *content);
