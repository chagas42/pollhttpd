# pollhttpd

A single-process HTTP/1.1 server written from scratch in C, built on `poll()`.

No frameworks, no dependencies — just POSIX sockets, a hand-written parser, and the
standard library. Written to understand *why* the protocol is shaped the way it is,
so every decision traces back to a clause in the specification.

```
$ make && ./pollhttpd
[server] listening on http://localhost:8080
```

## What it does

- **Dual-stack listener.** A single socket bound to `::` with `IPV6_V6ONLY` disabled
  accepts both IPv6 and IPv4 clients.
- **Byte-at-a-time request parser.** A 24-state machine that survives arbitrary
  fragmentation — the same request split at any position parses identically.
- **Static file serving** from `www/`, with layered path traversal protection.
- **Single-process concurrency** via a `poll()` event loop. A slow client cannot
  stall a fast one.
- **Persistent connections**, request body framing (`Content-Length` and `chunked`),
  and the RFC 9112 requirements most hand-rolled servers miss.

## Build and run

```bash
make            # build with -Wall -Wextra
./pollhttpd   # listen on :8080, serving ./www
make test       # run the test suite
make memcheck   # run under valgrind
make clean
```

Requires a C11 compiler and a POSIX system. Tested on Linux.

## Architecture

```
src/main.c            entry point; ignores SIGPIPE, delegates
src/server.c          listening socket and the poll() event loop
src/connection.c      per-connection state: parser, output buffer, lifecycle
src/handler.c         request to response: methods, status codes
src/http_parser.c     the state machine: request line, headers, body
src/http_response.c   response serialization
src/files.c           request target to safe path to content
```

Each module exposes a handful of symbols; everything else is `static`. The parser
never sees a file descriptor — it consumes bytes, which is what makes it testable
without a socket.

## Conformance

| Behavior | Clause |
|---|---|
| `Host` required, exactly once, on HTTP/1.1 | RFC 9112 §3.2 |
| Body framing: `Content-Length` vs `chunked` | RFC 9112 §6.3 |
| Chunked transfer decoding | RFC 9112 §7.1 |
| `Content-Length` + `Transfer-Encoding` together → close the connection | RFC 9112 §6.1 |
| Persistent connections by default | RFC 9112 §9.3 |
| Honor `Connection: close` | RFC 9112 §9.6 |
| `Date` header in IMF-fixdate format | RFC 9110 §6.6.1 |
| `HEAD` returns the same headers as `GET`, without a body | RFC 9110 §9.3.2 |

Status codes: `200`, `400`, `403`, `404`, `405`, `408`, `413`, `431`, `501`.

## Two design notes

**Path traversal takes two layers, because neither catches everything.**
`realpath()` fails with `ENOENT` on a missing file, so it cannot distinguish a
legitimate `404` from an escape attempt — which is why the target is normalized
lexically first. But lexical normalization cannot see through a symlink pointing
outside the root — which is why `realpath()` runs after, and the result is checked
against the resolved root. Percent-decoding happens before both, since `%2e%2e`
only becomes `..` once decoded.

**Incomplete is not an error.** `read()` returns whatever bytes happened to arrive,
cut at a position the network chose. The parser's `INCOMPLETE` result is the normal
state of a machine fed in fragments; treating it as failure defeats the purpose.
The test suite feeds the same request split at every possible offset and asserts an
identical result.

## Testing

```bash
make test
```

A dependency-free harness in `test/harness.h`. Failures report file and line and the
run continues, so one pass shows everything that broke.

## References

- [RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [RFC 9112 — HTTP/1.1 Message Syntax and Routing](https://www.rfc-editor.org/rfc/rfc9112.html)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

## Status

A learning project, not production software. It has no TLS, no HTTP/2, no access log,
and no configuration file.
