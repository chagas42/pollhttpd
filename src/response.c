/* response.c — Milestone 2/4.
 * TODO(M2): montar status-line "HTTP/1.1 <code> <reason>\r\n",
 *   headers (Content-Length OBRIGATÓRIO, Content-Type, Date),
 *   linha vazia, corpo. Terminador SEMPRE \r\n.
 * TODO(M4): para HEAD (headers_only), emitir os mesmos headers do GET
 *   sem o corpo.
 * Atenção: send()/write() podem aceitar menos bytes do que pedido —
 *   faça loop até escrever tudo.
 */
#include "response.h"

int response_send(int fd, int status_code, const char *content_type,
                  const char *body, size_t body_len, int headers_only) {
    (void)fd;
    (void)status_code;
    (void)content_type;
    (void)body;
    (void)body_len;
    (void)headers_only;
    /* TODO: implementar conforme Milestone 2/4. */
    return -1;
}
