/* files.c — Milestone 4.
 * TODO(M4):
 *   files_resolve: juntar root+target, realpath(), e VERIFICAR que o
 *     resultado começa com root canônico (senão 403). Inexistente -> 404.
 *   files_content_type: tabela extensão -> media type; default
 *     "application/octet-stream".
 *   Leitura: open()/fstat() para Content-Length exato, read() em loop.
 */
#include "files.h"

int files_resolve(const char *root, const char *target,
                  char *out, unsigned long out_size) {
    (void)root;
    (void)target;
    (void)out;
    (void)out_size;
    /* TODO: implementar conforme Milestone 4. */
    return -1;
}

const char *files_content_type(const char *path) {
    (void)path;
    /* TODO: implementar conforme Milestone 4. */
    return "application/octet-stream";
}
