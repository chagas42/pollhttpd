/* Resolves a request target to file content without escaping the root. */
#pragma once

#include <stddef.h>

#define FILE_MAX_SIZE (1024 * 1024)

typedef enum {
    FILE_OK,
    FILE_NOT_FOUND,    /* -> 404 */
    FILE_FORBIDDEN,    /* -> 403: escapou da raiz, ou e diretorio sem index */
    FILE_TOO_LARGE,    /* -> 413 */
    FILE_BAD_TARGET,   /* -> 400: percent-encoding invalido, byte de controle */
} file_result;

typedef struct {
    char       *data;
    size_t      len;
    const char *media_type;
} file_content;

/* Loads the file `target` points to under `root`. On FILE_OK the caller
 * owns `out->data` and must call file_content_free. */
file_result file_load(const char *root, const char *target, file_content *out);

/* Releases the content. Safe to call on already-empty content. */
void file_content_free(file_content *content);

/* Media type from the extension. Never returns NULL. */
const char *file_media_type(const char *path);
