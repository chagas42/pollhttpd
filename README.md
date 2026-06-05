# http-server-c

Servidor HTTP/1.1 implementado do zero em C, em conformidade estrita com as especificações oficiais. Projeto de estudo para consolidar fundamentos de **Systems Programming**: sockets POSIX, parsing byte a byte, I/O de disco e multiplexação de I/O.

> **Filosofia do projeto:** não se trata de "fazer funcionar no curl". Trata-se de entender *por que* o protocolo é como é. Cada decisão de implementação deve ser rastreável a uma cláusula de RFC.

## Conformidade obrigatória

Toda mensagem produzida e consumida pelo servidor deve respeitar:

- **[RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)**: significado de métodos, códigos de status, headers de representação (semântica).
- **[RFC 9112 — HTTP/1.1 Message Syntax and Routing](https://www.rfc-editor.org/rfc/rfc9112.html)**: como os bytes são organizados na rede (sintaxe).

A distinção é central: **9112 é a gramática (sintaxe), 9110 é o vocabulário (semântica).** Você vai consultar as duas o tempo todo.

### Por que seguir a RFC muda o projeto

- **Gramática ABNF.** As RFCs definem cada construção em [ABNF (RFC 5234)](https://www.rfc-editor.org/rfc/rfc5234.html). O `CRLF` (`\r\n`) obrigatório ao fim de cada linha não é detalhe estético: um navegador rejeita respostas que não o respeitam.
- **Tratamento de erro.** A RFC 9112 especifica como reagir a requisições malformadas (ex.: `400 Bad Request`). Isso é fronteira de segurança — request smuggling nasce de parsers permissivos.
- **Semântica de métodos.** A RFC 9110 explica a diferença real entre `GET` e `HEAD` (o `HEAD` retorna os mesmos headers do `GET`, mas sem corpo) e por que ambos são obrigatórios.

## Roteiro de aprendizado

As 5 milestones vivem como **issues no GitHub** (não neste README). Cada uma traz objetivo, o que entender, critério de pronto e referências (Beej PT-BR, RFC 9110/9112, man7.org). Acompanhe e marque o progresso por lá:

| # | Milestone | Foco |
|---|-----------|------|
| [#1](../../issues/1) | O Aperto de Mão TCP | socket passivo IPv4/IPv6 via `getaddrinfo` |
| [#2](../../issues/2) | Resposta Estática RFC-Compliant | `200 OK` com status-line + headers + `CRLF` |
| [#3](../../issues/3) | Parser de Requisição com FSM | request-line + headers byte a byte; `400` em malformado |
| [#4](../../issues/4) | Sistema de Arquivos e Semântica | servir `www/`, `Content-Type`/`Length`, `GET`/`HEAD`, `404` |
| [#5](../../issues/5) | Concorrência de Sistemas | event loop `poll()` não-bloqueante, multi-cliente |

> Veja todas em **[Issues → label `milestone`](../../issues?q=is%3Aissue+label%3Amilestone)**.

## Estrutura do repositório

```
http-server-c/
├── README.md   # este arquivo: visão geral + índice das milestones
├── Makefile    # build com -Wall -Wextra -g + alvo de valgrind (make memcheck)
├── .gitignore
├── src/        # SUA implementação (vazio — você escreve do zero)
├── www/        # raiz dos arquivos estáticos servidos
│   └── index.html
└── docs/       # suas anotações de estudo
```

> `src/` está intencionalmente **vazio**. Este repo é um guia, não um esqueleto pronto: a implementação é sua. Sugestão de organização à medida que avança — `server.c` (M1), `response.c` (M2/M4), `http_parser.c` (M3), `files.c` (M4), `main.c`/event loop (M5) — mas a divisão fica a seu critério.

## Build e execução

```bash
make            # compila ./http-server a partir de src/*.c
./http-server   # sobe o servidor (escolha a porta no seu código, ex.: 8080)
make memcheck   # roda sob valgrind com checagem total de leaks
make clean      # remove artefatos
```

> O `make` só funciona depois que você criar seus primeiros `.c` em `src/`.

Ferramentas de teste recomendadas: `curl -v`, `curl -I`, `nc` (netcat), `printf '...' | nc localhost 8080`, e o próprio navegador.

## Disciplina de engenharia (todas as milestones)

- **Compile sempre com `-Wall -Wextra`** e trate warning como erro. Em C, warning ignorado vira bug de produção.
- **Rode `make memcheck` (valgrind) em cada milestone.** Leaks e leituras inválidas se acumulam silenciosamente.
- **Cheque o retorno de toda syscall** (`socket`, `bind`, `accept`, `read`, `write`...) com `perror`/`strerror(errno)`.
- **Não confie em entrada da rede.** Todo byte vindo do cliente é hostil até prova em contrário.
- **Um commit por avanço significativo**, no formato Conventional Commits (ex.: `feat(server): bind passive socket via getaddrinfo`).

## Glossário rápido

| Termo | Significado |
|-------|-------------|
| ABNF | Augmented BNF (RFC 5234): notação formal da gramática das RFCs |
| CRLF | `\r\n` — terminador de linha obrigatório em HTTP |
| OWS | Optional WhiteSpace ao redor de valores de header |
| FSM | Finite State Machine — base do parser da Milestone 3 |
| Socket passivo | Socket em modo escuta, criado no servidor para aceitar conexões |
