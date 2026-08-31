CC      := cc
CFLAGS  := -Wall -Wextra -g -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS :=

TARGET  := pollhttpd
SRCDIR  := src
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(SRCS:.c=.o)

TESTDIR   := test
TEST_BIN  := $(TESTDIR)/run
TEST_SRCS := $(wildcard $(TESTDIR)/*.c)
LIB_SRCS  := $(filter-out $(SRCDIR)/main.c, $(SRCS))

.PHONY: all clean memcheck run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) $(LIB_SRCS) $(wildcard $(SRCDIR)/*.h) $(TESTDIR)/harness.h
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $(TEST_SRCS) $(LIB_SRCS) $(LDFLAGS)

memcheck: $(TARGET)
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         --error-exitcode=1 \
	         ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_BIN)
