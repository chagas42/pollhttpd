/* Entry point. Orchestrates and delegates; holds no protocol logic. */

#include <signal.h>
#include <stdlib.h>

#include "server.h"

#define DEFAULT_PORT "8080"

int main(void) {
    /* Without this, a client disconnecting mid-write kills the process
     * with SIGPIPE before any error handling runs. Ignored, write()
     * returns -1 with EPIPE and the normal error path applies. */
    signal(SIGPIPE, SIG_IGN);

    return server_run(DEFAULT_PORT) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
