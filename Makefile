# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
# Licensed under the MIT License. See LICENSE in project root.

CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
BUILD_DIR ?= build
CORTEX_MODEL ?= models/cortex_demo.cwm
DISK_IMG := $(BUILD_DIR)/disk.img
STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
HOST_TEST := $(BUILD_DIR)/test_cortex_host

KERNEL_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding \
	-fno-pic -fno-pie -fno-plt -fno-stack-protector -fno-builtin \
	-fno-asynchronous-unwind-tables -fno-exceptions -mcmodel=large -m64 \
	-msse2 -mno-red-zone -Iinclude -Isrc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

KERNEL_SRCS := \
	src/kernel/kernel64_main.c \
	src/kernel/runtime.c \
	src/kernel/cortex_boot.c \
	src/core/mem_arena.c \
	src/cortex/cwm.c \
	src/cortex/cortex.c \
	src/drivers/x86/ata_pio.c
KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/src/kernel/kernel_entry.o

.PHONY: all clean cortex-model test-cortex-host cortex-x86 run-cortex-x86 check-tools
all: cortex-x86

check-tools:
	@command -v $(CC) >/dev/null || { echo "missing: $(CC)"; exit 1; }
	@command -v nasm >/dev/null || { echo "missing: nasm"; exit 1; }
	@command -v $(OBJCOPY) >/dev/null || { echo "missing: $(OBJCOPY)"; exit 1; }
	@command -v python3 >/dev/null || { echo "missing: python3"; exit 1; }

$(STAGE1_BIN): src/boot/stage1.asm
	@mkdir -p $(BUILD_DIR)
	nasm -f bin $< -o $@

$(STAGE2_BIN): src/boot/stage2.asm
	@mkdir -p $(BUILD_DIR)
	nasm -f bin $< -o $@

$(KERNEL_ENTRY_OBJ): src/kernel/kernel_entry.S
	@mkdir -p $(dir $@)
	$(CC) -m64 -mno-red-zone -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJS) src/kernel/kernel_x86.ld
	@mkdir -p $(BUILD_DIR)
	$(CC) -nostdlib -no-pie -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack \
		-Wl,--no-omagic -Wl,--build-id=none -Wl,-T,src/kernel/kernel_x86.ld \
		-o $@ $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@bytes=$$(wc -c < $@); echo "# cortex-kernel: $$bytes bytes"; \
		test $$bytes -le 65024 || { echo "kernel staging image exceeds conservative 127-sector BIOS read limit"; exit 1; }

cortex-model:
	@test -f $(CORTEX_MODEL) || { \
		echo "$(CORTEX_MODEL) missing; generating requires Python + PyTorch"; \
		python3 tools/cortex_train_demo.py $(CORTEX_MODEL); \
	}

$(HOST_TEST): src/cortex/cwm.c src/cortex/cortex.c tests/test_cortex_host.c include/cortex/cwm.h include/cortex/cortex.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/cortex/cwm.c src/cortex/cortex.c tests/test_cortex_host.c -o $@

test-cortex-host: $(HOST_TEST) cortex-model
	@out="$$( $(HOST_TEST) $(CORTEX_MODEL) 'cell> hello' )"; printf '%s\n' "$$out"; \
		printf '%s\n' "$$out" | grep -q 'Hello. I am Cell Cortex' || { echo '#CORTEX host inference FAIL'; exit 1; }
	@echo '#CORTEX host inference PASS'

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(CORTEX_MODEL) scripts/pack_disk.py
	@mkdir -p $(BUILD_DIR)
	python3 scripts/pack_disk.py $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $@ --model $(CORTEX_MODEL)

cortex-x86: check-tools test-cortex-host $(DISK_IMG)
	@echo '#CORTEX x86 image ready:' $(DISK_IMG)

run-cortex-x86: cortex-x86
	@command -v qemu-system-x86_64 >/dev/null || { echo 'missing: qemu-system-x86_64'; exit 1; }
	@scripts/run_cortex_x86.sh $(DISK_IMG)

clean:
	rm -rf $(BUILD_DIR)
