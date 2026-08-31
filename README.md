# pollhttpd

An HTTP/1.1 server written from scratch in C, on top of `poll()`. No frameworks, no
dependencies — POSIX sockets, a hand-written parser, and the standard library.

A study project. I wrote it to understand syscalls, the HTTP protocol as the RFCs
define it, how memory works underneath, and how one process serves many connections at
once. The long version is in [docs/why.md](docs/why.md).

```bash
make          # build with -Wall -Wextra
./pollhttpd   # serve ./www on :8080
make test     # run the test suite
make memcheck # run under valgrind
```

Requires a C11 compiler and a POSIX system. Tested on Linux.

## What it does

- One dual-stack socket on `::` serving both IPv6 and IPv4
- A 24-state parser fed one byte at a time, so arbitrary fragmentation parses the same
- Static files from `www/`, with two-layer path traversal protection
- `poll()` event loop: a slow client can't stall a fast one
- Persistent connections, `Content-Length` and chunked body framing, idle and request
  timeouts

Conforms to the RFC 9112 rules that are easy to miss: mandatory `Host` (§3.2), body
framing (§6.3), closing on `Content-Length` together with `Transfer-Encoding` (§6.1),
and honoring `Connection: close` (§9.6).

Status codes: `200`, `400`, `403`, `404`, `405`, `408`, `413`, `431`, `501`.

## Layout

```
src/main.c            entry point
src/server.c          listening socket and the poll() loop
src/connection.c      per-connection state and lifecycle
src/handler.c         request to response
src/http_parser.c     the state machine
src/http_response.c   response serialization
src/files.c           request target to safe path to content
```

## References

- [RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [RFC 9112 — HTTP/1.1 Message Syntax and Routing](https://www.rfc-editor.org/rfc/rfc9112.html)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [Maziero, *Sistemas Operacionais: Conceitos e Mecanismos*](https://wiki.inf.ufpr.br/maziero/doku.php?id=socm:start) (UFPR, free)

## Status

A learning project, not production software. No TLS, no HTTP/2, no access log, no
config file.
