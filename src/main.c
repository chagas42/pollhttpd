#include <signal.h>
#include <stdlib.h>

#include "server.h"

#define DEFAULT_PORT "8080"

int main(void) {

    signal(SIGPIPE, SIG_IGN);

    return server_run(DEFAULT_PORT) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
