CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -I../include -I../include/libc -I..

TEST := kernel-host-tests
SOURCES := test_main.c host_stubs.c ../kernel/mpmc.c ../kernel/handles.c \
           ../kernel/spinlock.c ../drivers/gpu/core/fence.c

.PHONY: all run clean

all: $(TEST)

$(TEST): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

run: $(TEST)
	./$(TEST)

clean:
	rm -f $(TEST)
