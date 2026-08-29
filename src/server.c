#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>

void send_all(int fd, const char *data, size_t len) {
    size_t total_sent = 0;
    while(total_sent < len) {
        //write data to socket, update total_sent
        ssize_t sent = write(fd, data + total_sent, len - total_sent);
        if(sent == -1 ) {
            perror("write");
            return;
        }
        total_sent += sent;
    }
}

int run() {
    struct addrinfo hints, *res, *pointer_addrinfo;
    int file_descriptor_socket, addr_or_err;
    //clear bytes on hints struct
    memset(&hints, 0, sizeof(hints));
    //set hints struct fields
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //getaddrinfo to get the address info for the socket
    addr_or_err= getaddrinfo(NULL, "8080", &hints, &res);

    //if getaddrinfo fails, print error and return
    if(addr_or_err) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_or_err));
        return 1;
    }

    pointer_addrinfo = res;
    int opt_val = 1;

    //loop through the address info and bind to the first valid address
    while(pointer_addrinfo) {
        //create socket
        file_descriptor_socket = socket(pointer_addrinfo->ai_family, pointer_addrinfo->ai_socktype, pointer_addrinfo->ai_protocol);


        //set socket options to allow address reuse
        setsockopt(file_descriptor_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

        if(file_descriptor_socket == -1 || bind(file_descriptor_socket, pointer_addrinfo->ai_addr, pointer_addrinfo->ai_addrlen) == -1) {
            perror("bind");
            close(file_descriptor_socket);
            pointer_addrinfo = pointer_addrinfo->ai_next;
        } else {
            break;
        }

    }
    //
    if(!pointer_addrinfo) {
        freeaddrinfo(res);
        return 1;
    }

    if(listen(file_descriptor_socket, 128) == -1) {
        perror("listen");
        close(file_descriptor_socket);
        freeaddrinfo(res);
        return 1;
    }

    //main loop conection
    while(1){
        int fd_result = accept(file_descriptor_socket, NULL, NULL);

        if(fd_result == -1) {
            perror("accept");
            continue;
        }
        printf("Connection accepted\n");
        const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nContent-Type: text/plain\r\n\r\nHello, World!";
        send_all(fd_result, response, strlen(response));
        close(fd_result);

    }

    //free memory and return
    freeaddrinfo(res);
    return 0;
}

int main() {
    return run();
}
