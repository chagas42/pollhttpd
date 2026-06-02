# http-server-c

Servidor HTTP/1.1 implementado do zero em C, em conformidade estrita com as especificações oficiais. Projeto de estudo para consolidar fundamentos de **Systems Programming**: sockets POSIX, parsing byte a byte, I/O de disco e multiplexação de I/O.

> **Filosofia do projeto:** não se trata de "fazer funcionar no curl". Trata-se de entender *por que* o protocolo é como é. Cada decisão de implementação deve ser rastreável a uma cláusula de RFC.

## Conformidade obrigatória

Toda mensagem produzida e consumida pelo servidor deve respeitar:

- **[RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)**: significado de métodos, códigos de status, headers de representação (semântica).
- **[RFC 9112 — HTTP/1.1 Message Syntax and Routing](https://www.rfc-editor.org/rfc/rfc9112.html)**: como os bytes são organizados na rede (sintaxe).

A distinção entre as duas é central no projeto: **9112 é a gramática (sintaxe), 9110 é o vocabulário (semântica).** Você vai consultar as duas o tempo todo.

### Por que seguir a RFC muda o projeto

- **Gramática ABNF.** As RFCs definem cada construção em [ABNF (RFC 5234)](https://www.rfc-editor.org/rfc/rfc5234.html). O `CRLF` (`\r\n`) obrigatório ao fim de cada linha não é detalhe estético: um navegador rejeita respostas que não o respeitam. Seu parser precisa tratar a gramática literalmente.
- **Tratamento de erro.** A RFC 9112 especifica como reagir a requisições malformadas (ex.: `400 Bad Request`). Isso é fronteira de segurança, não cosmético — request smuggling nasce de parsers permissivos.
- **Semântica de métodos.** A RFC 9110 explica a diferença real entre `GET` e `HEAD` (o `HEAD` retorna os mesmos headers do `GET`, mas sem corpo) e por que ambos são obrigatórios.

## Estrutura do repositório

```
http-server-c/
├── README.md            # este arquivo: o roteiro das 5 milestones
├── Makefile             # build com -Wall -Wextra -g + alvo de valgrind
├── .gitignore
├── src/                 # implementação (esqueletos a preencher por milestone)
│   ├── main.c           #   ponto de entrada / loop principal
│   ├── server.c/.h      #   M1: socket passivo (getaddrinfo/bind/listen)
│   ├── http_parser.c/.h #   M3: FSM da Request-Line + headers
│   ├── response.c/.h    #   M2/M4: serialização de Status-Line + headers
│   └── files.c/.h       #   M4: I/O de disco + Content-Type/Content-Length
├── www/                 # raiz dos arquivos estáticos servidos
│   └── index.html
├── docs/                # anotações de estudo por milestone
└── tests/               # scripts de teste (curl, nc, etc.)
```

> Os arquivos `.c` começam como **esqueletos com TODOs e protótipos**. O objetivo é você preencher a lógica seguindo as referências. Este README explica o *o quê*; as RFCs e os manuais explicam o *como*.

## Como compilar e rodar

```bash
make            # compila para ./http-server
./http-server   # sobe o servidor (porta default no código, ex.: 8080)
make memcheck   # roda sob valgrind com checagem total de leaks
make clean      # remove artefatos
```

Ferramentas de teste recomendadas: `curl -v`, `nc` (netcat), `printf '...' | nc localhost 8080`, e o próprio navegador.

---

# As 5 Milestones

Cada milestone tem: **objetivo**, **o que você precisa entender**, **critério de pronto** e **referências** (Beej PT-BR, RFC e `man7.org`). Não há código de solução — apenas o caminho.

## Milestone 1 — O Aperto de Mão TCP

**Objetivo.** Criar um *socket passivo* que escute conexões, independente de protocolo (IPv4 **ou** IPv6), sem hardcode de família de endereço.

**O que entender.**
- O fluxo canônico do lado servidor: `getaddrinfo()` → `socket()` → `bind()` → `listen()` → `accept()`. Cada passo tem um porquê.
- Por que usar `getaddrinfo()` com `AI_PASSIVE` em vez de preencher `struct sockaddr_in` na mão: ele resolve a lista de endereços e te dá independência de IPv4/IPv6. Você itera sobre os resultados e usa o primeiro que funcionar.
- `setsockopt(SO_REUSEADDR)` para evitar "Address already in use" ao reiniciar durante o desenvolvimento.
- O que `listen()` faz com o *backlog* e o que significa uma conexão estar na fila.
- Diferença entre o socket de escuta e o socket retornado por `accept()` (este último representa **uma** conexão).

**Critério de pronto.** O processo sobe, fica bloqueado em `accept()`, e ao conectar com `nc localhost 8080` você consegue logar que uma conexão chegou (sem ainda falar HTTP).

**Referências.**
- Beej PT-BR — seções de `getaddrinfo`, "Um servidor de stream simples": <https://beej.us/guide/bgnet/html/split-ptbr/>
- `man getaddrinfo(3)`: <https://man7.org/linux/man-pages/man3/getaddrinfo.3.html>
- `man socket(2)`: <https://man7.org/linux/man-pages/man2/socket.2.html>
- `man bind(2)`: <https://man7.org/linux/man-pages/man2/bind.2.html>
- `man listen(2)`: <https://man7.org/linux/man-pages/man2/listen.2.html>
- `man accept(2)`: <https://man7.org/linux/man-pages/man2/accept.2.html>
- `man setsockopt(2)`: <https://man7.org/linux/man-pages/man2/setsockopt.2.html>

## Milestone 2 — Resposta Estática RFC-Compliant

**Objetivo.** Após `accept()`, enviar uma resposta `200 OK` com "Hello, World" cuja sintaxe respeite a RFC 9112, sem nem olhar a requisição ainda.

**O que entender.**
- A estrutura de uma resposta HTTP/1.1 (RFC 9112 §2.1, "Message Format"):
  - **status-line** = `HTTP-version SP status-code SP [reason-phrase] CRLF`
  - **header fields**, cada um terminado por `CRLF`
  - uma linha **vazia** (`CRLF`) separando headers do corpo
  - o **corpo**
- O `CRLF` (`\r\n`) é obrigatório como terminador — `\n` sozinho é não-conforme.
- Headers mínimos para uma resposta sã: `Content-Length` (RFC 9110 §8.6) para o cliente saber onde o corpo termina, e por que ele importa para *keep-alive*. `Content-Type` (RFC 9110 §8.3) e `Date` (RFC 9110 §6.6.1, formato `IMF-fixdate`).
- A diferença entre escrever na *socket* com `send()`/`write()` e a necessidade de tratar escritas parciais (o kernel pode aceitar menos bytes do que você pediu).

**Critério de pronto.** `curl -v http://localhost:8080/` mostra status `200`, os headers que você definiu, e o corpo. O navegador renderiza sem reclamar de resposta malformada.

**Referências.**
- RFC 9112 §2.1 (Message Format) e §3 (Status Line): <https://www.rfc-editor.org/rfc/rfc9112.html#name-message-format>
- RFC 9110 §8.6 (Content-Length), §8.3 (Content-Type), §6.6.1 (Date): <https://www.rfc-editor.org/rfc/rfc9110.html#name-content-length>
- Beej PT-BR — `send()` e `recv()`: <https://beej.us/guide/bgnet/html/split-ptbr/>
- `man send(2)`: <https://man7.org/linux/man-pages/man2/send.2.html>

## Milestone 3 — Parser de Requisição com FSM

**Objetivo.** Ler a requisição do cliente e processá-la **byte a byte** com uma máquina de estados finitos (FSM), validando contra a gramática ABNF das RFCs.

**O que entender.**
- A **request-line** (RFC 9112 §3): `method SP request-target SP HTTP-version CRLF`.
- A gramática dos **header fields** (RFC 9112 §5): `field-name ":" OWS field-value OWS`. O que é `OWS` (optional whitespace), e por que você **não** pode confiar no formato.
- Por que uma FSM: você recebe bytes em pedaços arbitrários (`recv()` não respeita fronteiras de linha). A FSM mantém estado entre chamadas — estados típicos: `METHOD`, `TARGET`, `VERSION`, `HEADER_NAME`, `HEADER_VALUE`, `BODY`, `DONE`, `ERROR`.
- Robustez/segurança: limites de tamanho de linha e de número de headers (proteção contra requisições gigantes), rejeição de bytes de controle, tratamento de `CRLF` vs `LF` solto. Toda entrada malformada → `400 Bad Request` (RFC 9112 §2.2).
- Distinguir `method`s (RFC 9110 §9): no mínimo `GET` e `HEAD`; responder `501 Not Implemented` ou `405 Method Not Allowed` para o resto, conforme o caso.

**Critério de pronto.** Você consegue: parsear uma requisição real do navegador; extrair método, alvo, versão e a lista de headers; e devolver `400` para entradas quebradas (teste com `printf 'GET /\r\n\r\n' | nc` e variações inválidas).

**Referências.**
- RFC 9112 §3 (Request Line), §5 (Field Syntax), §2.2 (Message Parsing): <https://www.rfc-editor.org/rfc/rfc9112.html#name-request-line>
- RFC 9110 §9 (Methods), §5 (Fields): <https://www.rfc-editor.org/rfc/rfc9110.html#name-methods>
- ABNF — RFC 5234: <https://www.rfc-editor.org/rfc/rfc5234.html>
- `man recv(2)`: <https://man7.org/linux/man-pages/man2/recv.2.html>

## Milestone 4 — Sistema de Arquivos e Semântica

**Objetivo.** Servir arquivos estáticos a partir de `www/`, integrando I/O de disco aos headers de representação corretos da RFC 9110.

**O que entender.**
- Mapear `request-target` → caminho em disco com segurança. **Path traversal** (`../../etc/passwd`) é a falha clássica: canonicalize o caminho (`realpath()`) e rejeite o que escapar da raiz servida.
- Abrir e ler o arquivo: `open()`/`fstat()`/`read()` ou `stat()` para descobrir tamanho. Use `fstat()` para preencher `Content-Length` exato.
- `Content-Type`: mapear extensão → media type (`text/html`, `text/css`, `application/json`...). RFC 9110 §8.3.
- Semântica de status: `200 OK` quando existe; `404 Not Found` quando não; `403 Forbidden` para acesso negado; `405`/`501` para método não suportado.
- `HEAD` vs `GET` (RFC 9110 §9.3.1–9.3.2): `HEAD` deve produzir **exatamente os mesmos headers** que o `GET` correspondente (incluindo `Content-Length`), porém **sem corpo**. Implemente os dois compartilhando o caminho de código.
- (Opcional/avançado) `Last-Modified`, requisições condicionais e `304 Not Modified`.

**Critério de pronto.** `curl http://localhost:8080/index.html` retorna o arquivo com `Content-Type`/`Content-Length` corretos; `curl -I` (HEAD) retorna os mesmos headers sem corpo; caminhos inexistentes dão `404`; tentativas de `../` são bloqueadas.

**Referências.**
- RFC 9110 §8.3 (Content-Type), §8.6 (Content-Length), §9.3.1 GET / §9.3.2 HEAD, §15 (Status Codes): <https://www.rfc-editor.org/rfc/rfc9110.html#name-get>
- `man open(2)`: <https://man7.org/linux/man-pages/man2/open.2.html>
- `man stat(2)` / `fstat`: <https://man7.org/linux/man-pages/man2/stat.2.html>
- `man read(2)`: <https://man7.org/linux/man-pages/man2/read.2.html>
- `man realpath(3)`: <https://man7.org/linux/man-pages/man3/realpath.3.html>

## Milestone 5 — Concorrência de Sistemas

**Objetivo.** Atender múltiplos clientes simultâneos sem bloquear o processo, usando **multiplexação de I/O** (`poll()`/`select()`) — um único processo, sem thread por conexão.

**O que entender.**
- Por que `accept()` + `recv()` bloqueantes num laço só atende um cliente por vez: um cliente lento trava todos.
- Sockets **não-bloqueantes** (`fcntl(fd, O_NONBLOCK)`) e o significado de `EAGAIN`/`EWOULDBLOCK`.
- O modelo de event loop com `poll()`: manter um array de `struct pollfd`, registrar o socket de escuta + cada conexão, e reagir a `POLLIN`/`POLLOUT`. Por que `poll()` costuma ser preferível a `select()` (limite de `FD_SETSIZE`, ergonomia).
- Como isso conversa com a Milestone 3: como cada conexão é não-bloqueante, **a FSM por conexão precisa preservar estado entre fatias de leitura** — você não pode assumir que a requisição inteira chegou de uma vez. Cada `pollfd` carrega seu próprio buffer + estado de parser.
- Ciclo de vida da conexão: aceitar, ler até a requisição completar, responder, e fechar (ou manter viva, se for implementar `keep-alive` da RFC 9112 §9).
- (Avançado, fora de escopo mas bom citar) `epoll`/`kqueue` para escala maior; este projeto fica em `poll()` por portabilidade.

**Critério de pronto.** Dois clientes simultâneos (ex.: um `nc` "pendurado" sem enviar nada + um `curl` normal) são atendidos sem que o lento bloqueie o rápido. Sob `make memcheck`, nenhum leak ao fechar conexões.

**Referências.**
- Beej PT-BR — `poll()` e `select()`, "select()—Multiplexação síncrona de E/S": <https://beej.us/guide/bgnet/html/split-ptbr/>
- `man poll(2)`: <https://man7.org/linux/man-pages/man2/poll.2.html>
- `man select(2)`: <https://man7.org/linux/man-pages/man2/select.2.html>
- `man fcntl(2)`: <https://man7.org/linux/man-pages/man2/fcntl.2.html>
- RFC 9112 §9 (Connection Management / persistência): <https://www.rfc-editor.org/rfc/rfc9112.html#name-connection-management>

---

## Disciplina de engenharia (aplica-se a todas as milestones)

- **Compile sempre com `-Wall -Wextra`** e trate warning como erro. Em C, um warning ignorado vira um bug de produção.
- **Rode `make memcheck` (valgrind) em cada milestone.** Memory leaks e leituras inválidas se acumulam silenciosamente; pegue cedo.
- **Cheque o retorno de toda syscall.** `socket`, `bind`, `accept`, `read`, `write` falham. Use `perror()`/`strerror(errno)`.
- **Não confie em entrada da rede.** Todo byte que vem do cliente é hostil até prova em contrário (Milestone 3 e 4).
- **Um commit por avanço significativo**, mensagem no formato Conventional Commits (ex.: `feat(server): bind passive socket via getaddrinfo`).

## Glossário rápido

| Termo | Significado |
|-------|-------------|
| ABNF | Augmented BNF (RFC 5234): notação formal da gramática das RFCs |
| CRLF | `\r\n` — terminador de linha obrigatório em HTTP |
| OWS | Optional WhiteSpace ao redor de valores de header |
| FSM | Finite State Machine — base do parser da Milestone 3 |
| Socket passivo | Socket em modo escuta, criado no servidor para aceitar conexões |
