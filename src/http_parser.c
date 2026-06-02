/* http_parser.c — Milestone 3.
 * TODO(M3): implementar a FSM byte a byte.
 *   - request-line: method SP request-target SP HTTP-version CRLF
 *   - header:       field-name ":" OWS field-value OWS CRLF
 *   - fim dos headers: linha vazia (CRLF)
 *   - validar contra ABNF; impor limites; rejeitar bytes de controle.
 *   - qualquer violação -> PS_ERROR.
 */
#include "http_parser.h"

struct http_request {
    parser_state state;
    /* TODO: campos de method/target/version, headers, limites. */
};

parser_state http_parser_feed(http_request *req, const char *buf, size_t n) {
    (void)req;
    (void)buf;
    (void)n;
    /* TODO: implementar conforme Milestone 3. */
    return PS_ERROR;
}
