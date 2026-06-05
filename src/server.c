#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>

int main() {

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;



    int err = getaddrinfo(NULL, "8080", &hints, &res);
    if(err) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }

    p = res;
    int fd;
    while(p) {
        printf("%s\n", res->ai_addr->sa_data);
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if(fd == -1 || bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
            perror("bind");
            p = p->ai_next;
        } else {
            printf("bound to %s\n", res->ai_addr->sa_data);
            break;
        }

    }
    freeaddrinfo(res);
    return 0;
}
