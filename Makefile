ARCH ?= x86_64
TARGET ?= kernel.elf
BUILD_DIR ?= build
ifeq ($(ARCH),x86_64)
CROSS ?= x86_64-elf-
ARCH_CFLAGS := -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 -mno-mmx -mno-avx
else ifeq ($(ARCH),arm64)
CROSS ?= aarch64-elf-
ARCH_CFLAGS := -march=armv8-a
else ifeq ($(ARCH),riscv64)
CROSS ?= riscv64-unknown-elf-
ARCH_CFLAGS := -march=rv64gc -mabi=lp64
else
$(error Unsupported ARCH '$(ARCH)'; use x86_64, arm64, or riscv64)
endif

CC := clang -target x86_64-none-elf
LD := ld.lld
OBJCOPY := llvm-objcopy
NASM := nasm

CPPFLAGS := -Iinclude -Iinclude/libc -I. -DARCH_$(ARCH)
CFLAGS := -O2 -Wall -Wextra -std=c11 -ffreestanding -fno-builtin -nostdlib \
		   -fno-stack-protector $(ARCH_CFLAGS) $(CPPFLAGS)
ASFLAGS := -f elf64
WINDOW_OPS_AVX2 ?= 0
ifeq ($(WINDOW_OPS_AVX2),1)
WINDOW_OPS_ASFLAGS :=
else
WINDOW_OPS_ASFLAGS := -D WINDOW_OPS_NO_AVX2
endif
LDFLAGS := -m elf_x86_64 -z max-page-size=0x1000 -T linker.ld

C_DIRS := kernel mm fs hal lib drivers modules user arch/$(ARCH)
ASM_DIRS := boot arch/$(ARCH) user

C_SRCS := $(shell find $(C_DIRS) -type f -name '*.c' 2>/dev/null)
ASM_SRCS := $(shell find $(ASM_DIRS) -type f \( -name '*.asm' -o -name '*.S' \) 2>/dev/null)

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.asm,$(BUILD_DIR)/%_asm.o,$(ASM_SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(C_OBJS) $(ASM_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	$(OBJCOPY) -O binary $@ kernel.bin

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%_asm.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) kernel.bin
