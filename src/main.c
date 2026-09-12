#include <signal.h>
#include <stdlib.h>

#include "server.h"

int main(void) {

    signal(SIGPIPE, SIG_IGN);

    server_config cfg = server_config_defaults();

    return server_run(&cfg) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
