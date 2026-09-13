#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "harness.h"
#include "server.h"

#define T0 1700000000   // a fixed "now"; the clock only moves when a test says so

static void given_root(char *out, size_t cap) {
    char base[] = "/tmp/httpc-srv-XXXXXX";
    char *dir = mkdtemp(base);

    char path[512];
    snprintf(path, sizeof(path), "%s/index.html", dir);
    FILE *f = fopen(path, "w");
    fputs("<h1>ok</h1>", f);
    fclose(f);

    snprintf(out, cap, "%s", dir);
}

static server_config test_config(const char *root) {
    server_config cfg = server_config_defaults();
    cfg.port            = "0";      // ephemeral, so tests never collide
    cfg.root            = root;
    cfg.max_connections = 4;
    cfg.poll_timeout_ms = 5;        // tests pump many ticks; don't sleep a second each
    return cfg;
}

static int connect_to(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((uint16_t)port),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(fd);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

static void pump(server *s, time_t now, int ticks) {
    while (ticks-- > 0) {
        server_tick(s, now);
    }
}

// returns bytes read; sets *eof when the peer closed
static size_t drain(int fd, char *buf, size_t cap, int *eof) {
    size_t total = 0;
    *eof = 0;

    while (total + 1 < cap) {
        ssize_t n = recv(fd, buf + total, cap - total - 1, 0);
        if (n == 0) { *eof = 1; break; }
        if (n == -1) break;
        total += (size_t)n;
    }

    buf[total] = '\0';
    return total;
}

static int count_responses(const char *buf) {
    int n = 0;
    const char *p = buf;
    while ((p = strstr(p, "HTTP/1.1 ")) != NULL) { n++; p += 9; }
    return n;
}

void test_server(void) {
    signal(SIGPIPE, SIG_IGN);

    char root[256];
    given_root(root, sizeof(root));

    TEST("serves a file end to end");
    {
        server_config cfg = test_config(root);
        server *s = NULL;
        CHECK_INT(server_listen(&cfg, &s), 0);
        CHECK(server_port(s) > 0);

        int c = connect_to(server_port(s));
        CHECK(c != -1);
        send(c, "GET / HTTP/1.1\r\nHost: x\r\n\r\n", 27, 0);

        pump(s, T0, 6);

        char buf[8192]; int eof;
        drain(c, buf, sizeof(buf), &eof);
        CHECK(strstr(buf, "HTTP/1.1 200 OK") != NULL);
        CHECK(strstr(buf, "<h1>ok</h1>") != NULL);

        close(c);
        server_stop(s);
    }

    TEST("two pipelined requests get two responses");
    {
        server_config cfg = test_config(root);
        server *s = NULL;
        server_listen(&cfg, &s);

        int c = connect_to(server_port(s));
        const char *two = "GET / HTTP/1.1\r\nHost: x\r\n\r\n"
                          "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
        send(c, two, strlen(two), 0);

        pump(s, T0, 10);

        char buf[8192]; int eof;
        drain(c, buf, sizeof(buf), &eof);
        CHECK_INT(count_responses(buf), 2);

        close(c);
        server_stop(s);
    }

    TEST("a trickling client cannot hold a slot forever");
    {
        server_config cfg = test_config(root);
        cfg.request_timeout_s = 10;
        server *s = NULL;
        server_listen(&cfg, &s);

        int c = connect_to(server_port(s));
        send(c, "GET / HTTP/1.1\r\n", 16, 0);

        // one byte every 8s, always under the 10s limit
        time_t now = T0;
        for (int i = 0; i < 5; i++) {
            now += 8;
            send(c, "X", 1, 0);
            pump(s, now, 3);
        }

        // 40s after the request started: the deadline is long gone
        char buf[8192]; int eof;
        drain(c, buf, sizeof(buf), &eof);
        CHECK(eof == 1 || strstr(buf, "408") != NULL);

        close(c);
        server_stop(s);
    }

    TEST("at capacity, extra clients wait in the backlog");
    {
        server_config cfg = test_config(root);
        cfg.max_connections = 2;
        server *s = NULL;
        server_listen(&cfg, &s);

        int a = connect_to(server_port(s));
        int b = connect_to(server_port(s));
        int c = connect_to(server_port(s));
        CHECK(a != -1 && b != -1 && c != -1);

        pump(s, T0, 8);

        // the third must still be pending, not reset in its face
        char buf[1024]; int eof;
        drain(c, buf, sizeof(buf), &eof);
        CHECK_INT(eof, 0);

        close(a); close(b); close(c);
        server_stop(s);
    }

    TEST("a stalled request gets a 408");
    {
        server_config cfg = test_config(root);
        cfg.request_timeout_s = 10;
        server *s = NULL;
        server_listen(&cfg, &s);

        int c = connect_to(server_port(s));
        send(c, "GET / HTTP/1.1\r\nHost: x\r\n", 25, 0);
        pump(s, T0, 3);
        pump(s, T0 + 30, 3);

        char buf[4096]; int eof;
        drain(c, buf, sizeof(buf), &eof);
        CHECK(strstr(buf, "408") != NULL);

        close(c);
        server_stop(s);
    }
}
