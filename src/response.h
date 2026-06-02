/* response.h — Milestone 2/4: serialização de respostas RFC 9112.
 * Ref: RFC 9112 §2.1 (Message Format), §3 (Status Line);
 *      RFC 9110 §8.3/§8.6/§6.6.1 (Content-Type/Content-Length/Date).
 */
#ifndef RESPONSE_H
#define RESPONSE_H

#include <stddef.h>

/* Escreve uma resposta completa no fd:
 *   status-line CRLF (headers CRLF)* CRLF [body]
 * Lida com escritas parciais de send()/write().
 * Se body_only_headers != 0 (HEAD), envia headers sem corpo. */
int response_send(int fd,
                  int status_code,
                  const char *content_type,
                  const char *body,
                  size_t body_len,
                  int headers_only);

#endif /* RESPONSE_H */
