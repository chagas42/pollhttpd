#include <signal.h>
#include <stdlib.h>

#include "server.h"

int main(void) {

    signal(SIGPIPE, SIG_IGN);

    server_config cfg = server_config_defaults();

    int result = server_run(&cfg);

    if(result == 0){
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
