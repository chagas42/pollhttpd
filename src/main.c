/* main.c — ponto de entrada e loop principal.
 * Evolui ao longo das milestones:
 *   M1: server_listen() + accept() em laço bloqueante.
 *   M2: responder estático após accept (response_send).
 *   M3: alimentar bytes ao http_parser antes de responder.
 *   M4: roteamento para arquivos em www/ (files_*).
 *   M5: trocar o laço bloqueante por event loop com poll() (não-bloqueante);
 *       manter estado de parser por conexão. man poll(2), fcntl(2).
 */
#include "server.h"

#define DEFAULT_PORT "8080"
#define BACKLOG 16

int main(void) {
    /* TODO(M1): int lfd = server_listen(DEFAULT_PORT, BACKLOG); ... */
    return 0;
}
