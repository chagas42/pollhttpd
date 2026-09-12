CC      := cc
CFLAGS  := -Wall -Wextra -g -std=c11 -MMD -MP \
           -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS :=

TARGET  := pollhttpd
SRCDIR  := src
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(SRCS:.c=.o)
DEPS    := $(OBJS:.o=.d)

TESTDIR   := test
TEST_BIN  := $(TESTDIR)/run
TEST_SRCS := $(wildcard $(TESTDIR)/*.c)
LIB_SRCS  := $(filter-out $(SRCDIR)/main.c, $(SRCS))

.PHONY: all clean memcheck run test asan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) $(LIB_SRCS)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $(TEST_SRCS) $(LIB_SRCS) $(LDFLAGS)

# asan/ubsan acham em segundos o que o valgrind demora
asan:
	$(CC) $(CFLAGS) -I$(SRCDIR) -fsanitize=address,undefined \
	      -fno-omit-frame-pointer -o $(TESTDIR)/run-asan \
	      $(TEST_SRCS) $(LIB_SRCS) $(LDFLAGS)
	./$(TESTDIR)/run-asan

# o servidor nao termina sozinho: sem timeout o valgrind nunca reporta
memcheck: $(TARGET)
	-timeout 10 valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         ./$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) $(TEST_BIN) $(TESTDIR)/run-asan

-include $(DEPS)
