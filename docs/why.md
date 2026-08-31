# Why I built this

I write Node at work. `http.createServer` gives you a callback and the request
arrives already parsed, with a `headers` object and a `url` string. I never knew where
that came from.

So I built the smallest thing that would force me through the whole stack: an
HTTP/1.1 server in C, no libraries.

Four things I wanted out of it: understand syscalls, understand the HTTP protocol
properly by reading the RFCs, understand how memory works underneath, and understand
how a server handles many connections at once.

## Syscalls

`socket()` was the first thing that surprised me. It returns an int and I assumed that
int was the connection. It isn't. `socket()` doesn't touch the network at all. It
creates an object in the kernel and gives you back an index into a table. No IP, no
port, no bytes. The address only comes with `bind()`.

`accept()` surprised me again. I expected the connection to come back on the same
descriptor I was listening on. It doesn't. `accept()` creates a new one. The listening
socket stays in `LISTEN` with no peer, and each client gets its own descriptor. You
can see this in `ss`: the listening socket has no peer address, and every established
one has a different client port.

Then `write()`. It returns how many bytes the kernel took, not how many you asked it
to send. The send buffer is finite. When it fills you get a short write, and on a
non-blocking socket you get `EAGAIN` and zero bytes. Every write loop here exists
because of that.

## The protocol

I read RFC 9110 and RFC 9112 instead of copying examples.

Worth it, because the rules that matter are the ones you never hit testing with curl.
An HTTP/1.1 request with no `Host` has to be rejected. A server has to be able to
decode chunked encoding even if it never sends it. If a request has both
`Content-Length` and `Transfer-Encoding`, you respond and then close the connection,
because a proxy in front might frame the message differently than you do. That gap is
request smuggling.

You don't find any of that by testing until the browser stops complaining.

## The parser, and why it's a state machine

`read()` gives you whatever bytes arrived. Not a line. A chunk cut wherever the
network decided.

The same request can come as one read, or as `GET / HT`, then `TP/1.1\r\nHos`, then
`t: x\r\n\r\n`. There's no call for "give me a full line". So you can't write "read
the request line, then read the headers". You need something that can stop in the
middle of a word and pick up later knowing where it stopped.

That's all a state machine is. I didn't really get that until I had to write one. The
test that convinced me feeds the same request split at every possible position and
checks the result is identical.

## Memory

C makes you answer questions a garbage collector lets you skip.

Where does this live? The response body starts as a string literal in the binary's
read-only section. You can see it with `readelf -p .rodata`. Writing to it segfaults.
Who frees it? `getaddrinfo` allocates a list and the contract says you call
`freeaddrinfo`. How long does it live? A local array dies when the function returns.

The one that took me longest: state that has to survive between calls. A `static`
local does that, which looks like what a parser needs, until you have two clients and
find out there's only one of it for the whole program. That's why the parser here
lives inside a connection struct and gets passed in.

## Many connections at once

A blocking `accept` loop looks fine and works with one client. The problem is easy to
miss locally because everything is fast. What made it obvious was leaving a `nc` open
on the port without sending anything and watching a normal request wait behind it.

`poll()` flips who is in charge. Instead of asking one socket for data and blocking,
you give the kernel a list and ask which ones are ready. Nothing waits. The cost is
that each connection has to carry its own buffer and its own parser state, since the
loop can come back between any two bytes.

## Books

**Carlos A. Maziero, *Sistemas Operacionais: Conceitos e Mecanismos*** (UFPR, free
under Creative Commons). Where the ideas below the syscalls came from: processes, file
descriptors as a per-process table, what blocking actually costs. Reading the I/O
multiplexing chapter before writing the `poll()` loop was the difference between
copying a pattern and understanding it.

**Beej's Guide to Network Programming.** The practical side. `getaddrinfo`, the shape
of a stream server, partial sends. Section 7 is short and covers a lot.

**RFC 9110 and RFC 9112.** Denser than they look. 9112 is the grammar, 9110 is the
vocabulary.

## What I'd tell myself at the start

Write the state table before writing the parser. I lost a lot of time trying to design
the state machine and write C at the same time, which is two hard problems at once.
Four columns on paper, states down the side, and then the code is just transcription.

And the compiler warnings aren't noise. `-Wall -Wextra` caught a pointer to a dead
stack frame before I ever ran the thing.
