# Progresso das Milestones

Marque conforme avançar. Cada milestone só é "feita" quando passa no
critério de pronto **e** roda limpa sob `make memcheck`.

- [ ] **M1 — Aperto de mão TCP** — socket passivo IPv4/IPv6 via getaddrinfo
- [ ] **M2 — Resposta estática** — 200 OK RFC-9112 (status-line + headers + CRLF)
- [ ] **M3 — Parser FSM** — request-line + headers byte a byte; 400 em malformado
- [ ] **M4 — Arquivos + semântica** — servir www/, Content-Type/Length, GET/HEAD, 404
- [ ] **M5 — Concorrência** — event loop poll() não-bloqueante, multi-cliente

## Notas de estudo
<!-- Anote aqui descobertas, trechos de RFC que te pegaram, comandos de teste. -->
