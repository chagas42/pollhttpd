/* server.h — Milestone 1: socket passivo independente de protocolo.
 * Ref: RFC 9112; Beej PT-BR (getaddrinfo); man getaddrinfo(3), socket(2),
 * bind(2), listen(2), accept(2).
 */
#ifndef SERVER_H
#define SERVER_H

/* Cria um socket de escuta na porta dada (string, ex.: "8080"),
 * usando getaddrinfo + AI_PASSIVE para suportar IPv4/IPv6.
 * Retorna o fd de escuta, ou -1 em erro. */
int server_listen(const char *port, int backlog);

#endif /* SERVER_H */
