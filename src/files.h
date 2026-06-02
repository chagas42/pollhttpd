/* files.h — Milestone 4: I/O de disco + headers de representação.
 * Ref: RFC 9110 §8.3/§8.6/§9.3 (GET/HEAD)/§15 (status).
 *      man open(2), stat(2), read(2), realpath(3).
 */
#ifndef FILES_H
#define FILES_H

/* Resolve request-target -> caminho seguro sob a raiz (root).
 * DEVE bloquear path traversal (../) via realpath + checagem de prefixo.
 * Retorna 0 e preenche out em sucesso; <0 em erro (403/404). */
int files_resolve(const char *root, const char *target,
                  char *out, unsigned long out_size);

/* Mapeia extensão -> media type (text/html, text/css, ...). */
const char *files_content_type(const char *path);

#endif /* FILES_H */
