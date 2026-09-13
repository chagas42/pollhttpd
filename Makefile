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

.PHONY: all clean memcheck run test asan coverage compdb

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

# o clangd do editor nao le Makefile; sem este arquivo ele chuta as flags
compdb:
	@python3 tools/gen-compile-commands.py

COVDIR := build/cov

# which lines the tests actually run. src/files.c and test/files.c share a
# basename, so they must compile into separate directories or their .gcda
# files overwrite each other and report 0%.
coverage:
	@rm -rf $(COVDIR) && mkdir -p $(COVDIR)/src $(COVDIR)/test
	@for f in $(LIB_SRCS); do \
	    $(CC) $(CFLAGS) -I$(SRCDIR) --coverage -c $$f \
	          -o $(COVDIR)/src/`basename $$f .c`.o; \
	 done
	@for f in $(TEST_SRCS); do \
	    $(CC) $(CFLAGS) -I$(SRCDIR) --coverage -c $$f \
	          -o $(COVDIR)/test/`basename $$f .c`.o; \
	 done
	@$(CC) --coverage -o $(COVDIR)/run $(COVDIR)/src/*.o $(COVDIR)/test/*.o
	@$(COVDIR)/run > /dev/null 2>&1
	@gcov -n -o $(COVDIR)/src $(COVDIR)/src/*.gcda \
	   | grep -A1 "^File .*$(SRCDIR)/" | grep -v -- "--" \
	   | paste - - | sed "s|File '||;s|'\t|  |" | sort

clean:
	rm -rf build
	rm -f $(OBJS) $(DEPS) $(TARGET) $(TEST_BIN) $(TESTDIR)/run-asan

-include $(DEPS)
