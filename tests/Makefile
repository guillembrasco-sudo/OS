CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -I../include -I../include/libc -I..
ASM ?= nasm
ASFLAGS ?= -f elf64
WINDOW_OPS_AVX2 ?= 0
ifeq ($(WINDOW_OPS_AVX2),1)
WINDOW_OPS_ASFLAGS :=
else
WINDOW_OPS_ASFLAGS := -D WINDOW_OPS_NO_AVX2
endif

TEST := kernel-host-tests
SOURCES := test_main.c host_stubs.c ../kernel/mpmc.c ../kernel/handles.c \
           ../kernel/spinlock.c ../drivers/gpu/core/fence.c
WINDOW_SOURCE := ../kernel/window/window_system.c
CONSOLE_SOURCE := ../kernel/window/master_console.c
WINDOW_ASM_SOURCE := ../arch/x86_64/window_ops.asm
WINDOW_ASM_OBJECT := window_ops.o

.PHONY: all run clean

all: $(TEST)

$(TEST): $(SOURCES) $(WINDOW_SOURCE) $(CONSOLE_SOURCE) $(WINDOW_ASM_OBJECT)
	$(CC) $(CFLAGS) $(SOURCES) $(WINDOW_SOURCE) $(CONSOLE_SOURCE) $(WINDOW_ASM_OBJECT) -o $@

$(WINDOW_ASM_OBJECT): $(WINDOW_ASM_SOURCE)
	$(ASM) $(ASFLAGS) $(WINDOW_OPS_ASFLAGS) $< -o $@

run: $(TEST)
	./$(TEST)

clean:
	rm -f $(TEST) $(WINDOW_ASM_OBJECT)
