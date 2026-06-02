# Makefile — http-server-c
# Build voltado para depuração: warnings agressivos + símbolos de debug.

CC      := cc
# -Wall -Wextra: warnings agressivos | -g: símbolos para gdb/valgrind
# -std=c11: padrão estável | -D_POSIX_C_SOURCE: expõe getaddrinfo, etc.
CFLAGS  := -Wall -Wextra -g -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS :=

TARGET  := http-server
SRCDIR  := src
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean memcheck run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Regra implícita para cada objeto; recompila se o header mudar.
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# Depuração de memória com valgrind (Milestone-by-milestone).
# --leak-check=full: detalha cada leak | --show-leak-kinds=all: todos os tipos
# --track-origins=yes: origem de valores não inicializados | --error-exitcode=1
memcheck: $(TARGET)
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         --error-exitcode=1 \
	         ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
