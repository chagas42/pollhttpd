/* server.c — Milestone 1.
 * TODO(M1):
 *   1. struct addrinfo hints {.ai_family=AF_UNSPEC, .ai_socktype=SOCK_STREAM,
 *      .ai_flags=AI_PASSIVE}; getaddrinfo(NULL, port, &hints, &res).
 *   2. Iterar a lista: socket() -> setsockopt(SO_REUSEADDR) -> bind().
 *      Parar no primeiro que der certo; freeaddrinfo() ao final.
 *   3. listen(fd, backlog). Checar TODOS os retornos (perror).
 */
#include "server.h"

int server_listen(const char *port, int backlog) {
    (void)port;
    (void)backlog;
    /* TODO: implementar conforme Milestone 1. */
    return -1;
}
