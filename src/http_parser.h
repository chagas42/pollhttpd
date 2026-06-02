/* http_parser.h — Milestone 3: FSM da Request-Line + headers.
 * Ref: RFC 9112 §3 (Request Line), §5 (Field Syntax), §2.2 (Parsing);
 *      RFC 5234 (ABNF). man recv(2).
 */
#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>

/* Estados da máquina (preserva contexto entre fatias de recv). */
typedef enum {
    PS_METHOD,
    PS_TARGET,
    PS_VERSION,
    PS_HEADER_NAME,
    PS_HEADER_VALUE,
    PS_BODY,
    PS_DONE,
    PS_ERROR
} parser_state;

/* TODO(M3): definir struct com buffers para method/target/version,
 * lista de headers, limites de tamanho e o estado atual. */
typedef struct http_request http_request;

/* Alimenta n bytes ao parser. Retorna o estado resultante.
 * Entradas malformadas devem levar a PS_ERROR (-> 400 Bad Request). */
parser_state http_parser_feed(http_request *req, const char *buf, size_t n);

#endif /* HTTP_PARSER_H */
