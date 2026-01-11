# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
# Licensed under the MIT License. See LICENSE in project root.

CC ?= gcc
AARCH64_CC ?= aarch64-linux-gnu-gcc
LD ?= ld
BUILD_DIR ?= build
OUT ?= ../out
TMPDIR ?= /tmp
STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
STAGE2_ELF := $(BUILD_DIR)/stage2.elf
STAGE2_ASM_OBJ := $(BUILD_DIR)/stage2_asm.o
STAGE2_CGF_OBJ := $(BUILD_DIR)/cgf2_verify.o
STAGE2_SHA_OBJ := $(BUILD_DIR)/stage2_sha.o
CGF_BIN := $(BUILD_DIR)/cgf.bin
DISK_IMG := $(BUILD_DIR)/disk.img
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
KERNEL_MAIN_OBJ := $(BUILD_DIR)/kernel64_main.o
KERNEL_PROOFS_OBJ := $(BUILD_DIR)/kernel_proofs.o
ARM64_KERNEL_ELF := $(BUILD_DIR)/arm64_kernel.elf
ARM64_KERNEL_MAIN_OBJ := $(BUILD_DIR)/kernel64_main_arm.o
ARM64_KERNEL_PROOFS_OBJ := $(BUILD_DIR)/kernel_proofs_arm.o
ARM64_START_OBJ := $(BUILD_DIR)/arm64_start.o
KERNEL_CFLAGS := -ffreestanding -fno-pic -fno-plt -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -fno-exceptions -mcmodel=large -m64 -O2 -Iinclude -Isrc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
KERNEL_AARCH64_CFLAGS := -ffreestanding -fno-pic -fno-plt -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -fno-exceptions -O2 -Iinclude -Isrc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
KERNEL_LIB_SRCS := src/core/channel_graph.c src/core/cg.c src/core/qos.c \
	src/core/hash_stub.c src/core/organelles.c src/core/events.c \
	src/core/update.c src/core/obs.c src/core/obs_trace.c src/core/crypto.c \
	src/core/trust.c src/core/log_ring.c src/core/kv.c src/core/e0/substrate_db.c \
	src/core/e0/e0_probe.c src/core/e0/e0_log.c \
	libparcel/parcel.c src/genes/mito_energy.c src/genes/golgi_route.c \
	src/genes/lyso_cleanup.c src/genes/peroxi_sanitize.c src/genes/testgene.c
KERNEL_X86_LIB_OBJS := $(patsubst %.c,$(BUILD_DIR)/kernel_x86/%.o,$(KERNEL_LIB_SRCS))
KERNEL_ARM_LIB_OBJS := $(patsubst $(BUILD_DIR)/kernel_x86/%,$(BUILD_DIR)/kernel_arm64/%,$(KERNEL_X86_LIB_OBJS))
KERNEL_ARM_EXTRA_SRCS := src/arch/arm64/gicv2.c src/arch/arm64/timer.c src/kernel/pf12_arm_irq.c
KERNEL_ARM_EXTRA_OBJS := $(patsubst %.c,$(BUILD_DIR)/kernel_arm64/%.o,$(KERNEL_ARM_EXTRA_SRCS))
KERNEL_RUNTIME_OBJ := $(BUILD_DIR)/kernel_runtime.o
ARM64_KERNEL_RUNTIME_OBJ := $(BUILD_DIR)/kernel_runtime_arm.o
KERNEL_NOTE_OBJ := $(BUILD_DIR)/kernel_gnu_stack.o
ARM64_KERNEL_NOTE_OBJ := $(BUILD_DIR)/kernel_gnu_stack_arm.o
E0_CPUID_OBJ := $(BUILD_DIR)/e0_cpuid.o
E0_MIDR_OBJ := $(BUILD_DIR)/arm64_midr.o
SMOKE_DIR := src/smoke
PARCEL_OBJ := $(BUILD_DIR)/parcel.o
REPLAY_BIN := $(BUILD_DIR)/replay_trace
TEST_PARCEL := $(BUILD_DIR)/test_parcel
EMIT_PARCEL := $(BUILD_DIR)/emit_parcel
TEST_CG := $(BUILD_DIR)/test_cg
TEST_QOS := $(BUILD_DIR)/test_qos
TEST_SCHED := $(BUILD_DIR)/test_sched
TEST_LOG_RING := $(BUILD_DIR)/test_log_ring
TEST_KV := $(BUILD_DIR)/test_kv
TEST_E0_X86 := tests/e0/test_x86_qemu.sh
TEST_E0_ARM := tests/e0/test_arm64_qemu.sh
LDS_E3_X86 := $(SMOKE_DIR)/e3_x86.ld
LDS_E3_ARM := $(SMOKE_DIR)/e3_arm64.ld
QOS_HOST_OBJ := $(BUILD_DIR)/qos_host.o
CH0_LOG_OBJ := $(BUILD_DIR)/ch0_log.o
CFG_ENABLE_CGF_VERIFY ?= 0
GEN_CGF_FLAGS ?=
CFG_ENABLE_IRQ_TIMER ?= 0
CFG_ENABLE_NONBLOCK_LOG ?= 0
CFG_ENABLE_ORG_PKV ?= 0
FEATURE_FLAGS := -DCFG_ENABLE_IRQ_TIMER=$(CFG_ENABLE_IRQ_TIMER) -DCFG_ENABLE_NONBLOCK_LOG=$(CFG_ENABLE_NONBLOCK_LOG) -DCFG_ENABLE_ORG_PKV=$(CFG_ENABLE_ORG_PKV)
PF12_VECTOR_OBJ := $(BUILD_DIR)/pf12_arm_vectors.o
KERNEL_CFLAGS += $(FEATURE_FLAGS)
KERNEL_AARCH64_CFLAGS += $(FEATURE_FLAGS)
FORCE:

ORG_COMMON_SRCS := src/genes/mito_energy.c src/genes/golgi_route.c src/genes/lyso_cleanup.c src/genes/peroxi_sanitize.c
ORG_COMMON_HDRS := include/gene/organelle.h include/gene/mito_energy.h include/gene/golgi_route.h include/gene/lyso_cleanup.h include/gene/peroxi_sanitize.h
ORG_COMMON_OBJS := $(patsubst src/genes/%.c,$(BUILD_DIR)/genes/%.o,$(ORG_COMMON_SRCS))

.PHONY: kernel kernel-arm
kernel: $(KERNEL_BIN)
kernel-arm: $(ARM64_KERNEL_ELF)

$(STAGE1_BIN): src/boot/stage1.asm
	@mkdir -p $(BUILD_DIR)
	nasm -f bin $< -o $@

ifeq ($(CFG_ENABLE_CGF_VERIFY),1)
$(STAGE2_BIN): $(STAGE2_ELF)
	@objcopy -O binary $< $@

$(STAGE2_ELF): src/boot/stage2.asm src/boot/stage2.ld src/boot/cgf2_verify.c include/boot/cgf2.h src/core/hash_sha256.c
	@mkdir -p $(BUILD_DIR)
	nasm -f elf64 -dCFG_ENABLE_CGF_VERIFY=$(CFG_ENABLE_CGF_VERIFY) $< -o $(STAGE2_ASM_OBJ)
	$(CC) -ffreestanding -fno-pie -fno-stack-protector -nostdlib -O2 -Wall -Wextra -Iinclude -c src/boot/cgf2_verify.c -o $(STAGE2_CGF_OBJ)
	$(CC) -ffreestanding -fno-pie -fno-stack-protector -nostdlib -O2 -Wall -Wextra -Iinclude -c src/core/hash_sha256.c -o $(STAGE2_SHA_OBJ)
	$(LD) -T src/boot/stage2.ld -o $@ $(STAGE2_ASM_OBJ) $(STAGE2_CGF_OBJ) $(STAGE2_SHA_OBJ)
else
$(STAGE2_BIN): src/boot/stage2.asm
	@mkdir -p $(BUILD_DIR)
	nasm -f bin $< -o $@
endif

$(KERNEL_ENTRY_OBJ): src/kernel/kernel_entry.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $< -o $@

$(KERNEL_MAIN_OBJ): src/kernel/kernel64_main.c include/core/handoff.h src/kernel/console.h src/kernel/proofs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(KERNEL_PROOFS_OBJ): src/kernel/proofs.c include/core/handoff.h src/kernel/console.h src/kernel/proofs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_x86/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(KERNEL_RUNTIME_OBJ): src/kernel/runtime.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(KERNEL_NOTE_OBJ): src/kernel/gnu_stack_note.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $< -o $@

$(E0_CPUID_OBJ): src/boot/cpuid_stub.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(KERNEL_PROOFS_OBJ) $(KERNEL_RUNTIME_OBJ) $(KERNEL_NOTE_OBJ) $(E0_CPUID_OBJ) $(KERNEL_X86_LIB_OBJS) src/kernel/kernel_x86.ld
	@mkdir -p $(BUILD_DIR)
	$(CC) -nostdlib -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack -Wl,--no-omagic -Wl,-T,src/kernel/kernel_x86.ld -o $@ \
		$(KERNEL_ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(KERNEL_PROOFS_OBJ) $(KERNEL_RUNTIME_OBJ) $(KERNEL_NOTE_OBJ) $(E0_CPUID_OBJ) $(KERNEL_X86_LIB_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

$(ARM64_START_OBJ): src/boot/arm64_start.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -c $< -o $@

$(ARM64_KERNEL_MAIN_OBJ): src/kernel/kernel64_main.c include/core/handoff.h src/kernel/console.h src/kernel/proofs.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) $(KERNEL_AARCH64_CFLAGS) -c $< -o $@

$(ARM64_KERNEL_PROOFS_OBJ): src/kernel/proofs.c include/core/handoff.h src/kernel/console.h src/kernel/proofs.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) $(KERNEL_AARCH64_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_arm64/%.o: %.c
	@mkdir -p $(dir $@)
	$(AARCH64_CC) $(KERNEL_AARCH64_CFLAGS) -c $< -o $@

$(PF12_VECTOR_OBJ): src/kernel/pf12_arm_vectors.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -c $< -o $@

$(ARM64_KERNEL_RUNTIME_OBJ): src/kernel/runtime.c
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) $(KERNEL_AARCH64_CFLAGS) -c $< -o $@

$(ARM64_KERNEL_NOTE_OBJ): src/kernel/gnu_stack_note.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -c $< -o $@

$(E0_MIDR_OBJ): src/boot/arm64_midr.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -c $< -o $@

$(ARM64_KERNEL_ELF): $(ARM64_START_OBJ) $(ARM64_KERNEL_MAIN_OBJ) $(ARM64_KERNEL_PROOFS_OBJ) $(ARM64_KERNEL_RUNTIME_OBJ) $(ARM64_KERNEL_NOTE_OBJ) $(E0_MIDR_OBJ) $(PF12_VECTOR_OBJ) $(KERNEL_ARM_LIB_OBJS) $(KERNEL_ARM_EXTRA_OBJS) src/kernel/kernel_arm64.ld
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -nostdlib -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack -Wl,--no-omagic -Wl,-T,src/kernel/kernel_arm64.ld -o $@ \
		$(ARM64_START_OBJ) $(ARM64_KERNEL_MAIN_OBJ) $(ARM64_KERNEL_PROOFS_OBJ) $(ARM64_KERNEL_RUNTIME_OBJ) $(ARM64_KERNEL_NOTE_OBJ) $(E0_MIDR_OBJ) $(PF12_VECTOR_OBJ) $(KERNEL_ARM_LIB_OBJS) $(KERNEL_ARM_EXTRA_OBJS)

.PHONY: full
full:
	@$(MAKE) clean
	@$(MAKE) -s e12-cgf-verify
	@$(MAKE) -s e12-arm-irq
	@$(MAKE) -s e12-logring
	@$(MAKE) -s e12-kv

.PHONY: check-freeze
check-freeze:
	@test "$${LC_ALL}" = "C" || (echo "LC_ALL must be C"; exit 1)
	@test "$${TZ}" = "UTC" || (echo "TZ must be UTC"; exit 1)
	@test -n "$${SOURCE_DATE_EPOCH}" || (echo "SOURCE_DATE_EPOCH not set"; exit 1)
	@grep -q 'SOURCE_DATE_EPOCH' scripts/build_release.sh || (echo "Missing SDE in build_release.sh"; exit 1)

ABICHECK := $(BUILD_DIR)/abi_check

$(ABICHECK): src/core/abi_check.c include/abi/nci.h include/abi/cap.h include/abi/msg.h include/abi/errors.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $< -o $@

.PHONY: check-abi
check-abi: $(ABICHECK)
	@$(ABICHECK)

$(BUILD_DIR)/log_marker_x86.o: src/core/log_marker.c include/core/log_marker.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -m64 -D__x86_64__ -c $< -o $@

$(BUILD_DIR)/log_marker_arm64.o: src/core/log_marker.c include/core/log_marker.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -Iinclude -D__aarch64__ -c $< -o $@

$(BUILD_DIR)/cg.o: src/core/cg.c include/core/cg.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/cg.c -o $@

$(BUILD_DIR)/cg_bare.o: src/core/cg.c include/core/cg.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -c src/core/cg.c -o $@

$(BUILD_DIR)/qos.o: src/core/qos.c include/core/qos.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -c src/core/qos.c -o $@

$(QOS_HOST_OBJ): src/core/qos.c include/core/qos.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/qos.c -o $@

$(BUILD_DIR)/scheduler.o: src/core/scheduler.c include/core/scheduler.h include/core/qos.h include/core/organelles.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/scheduler.c -o $@

$(BUILD_DIR)/channel_graph.o: src/core/channel_graph.c include/core/channel.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(BUILD_DIR)/log_ring.o: src/core/log_ring.c include/core/log_ring.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(BUILD_DIR)/kv.o: src/core/kv.c include/core/kv.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(TEST_KV): tests/test_kv.c include/core/kv.h $(BUILD_DIR)/kv.o
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_kv.c $(BUILD_DIR)/kv.o -o $@

$(BUILD_DIR)/organelles.o: src/core/organelles.c include/core/organelles.h $(ORG_COMMON_HDRS) include/core/channel.h include/core/parcel.h include/core/ch0_log.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(CH0_LOG_OBJ): src/core/ch0_log.c include/core/ch0_log.h include/core/parcel.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(BUILD_DIR)/genes/%.o: src/genes/%.c $(ORG_COMMON_HDRS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(BUILD_DIR)/cg_arm64.o: src/core/cg.c include/core/cg.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -c src/core/cg.c -o $@

$(BUILD_DIR)/qos_arm64.o: src/core/qos.c include/core/qos.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -c src/core/qos.c -o $@

$(BUILD_DIR)/parcel_arm64.o: libparcel/parcel.c include/core/parcel.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -c libparcel/parcel.c -o $@

$(BUILD_DIR)/marker_start_x86.o: $(SMOKE_DIR)/marker_start_x86.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -ffreestanding -fno-pie -m64 -c $< -o $@

$(BUILD_DIR)/marker_main_x86.o: $(SMOKE_DIR)/marker_main.c include/core/log_marker.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -m64 -Iinclude -D__x86_64__ -c $< -o $@

$(BUILD_DIR)/marker_x86.elf: $(BUILD_DIR)/marker_start_x86.o $(BUILD_DIR)/marker_main_x86.o $(BUILD_DIR)/log_marker_x86.o
	$(CC) -nostdlib -static -no-pie -m64 -Wl,-Ttext=0x100000 -Wl,-e,_start -Wl,--build-id=none $^ -o $@

$(BUILD_DIR)/e1_start_x86.o: $(SMOKE_DIR)/e1_start_x86.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -ffreestanding -fno-pie -m64 -c $< -o $@

$(BUILD_DIR)/e1_main_x86.o: $(SMOKE_DIR)/e1_main.c include/core/log_marker.h include/abi/nci.h include/abi/cap.h include/abi/msg.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -m64 -Iinclude -D__x86_64__ -c $< -o $@

$(BUILD_DIR)/e1_x86.elf: $(BUILD_DIR)/e1_start_x86.o $(BUILD_DIR)/e1_main_x86.o $(BUILD_DIR)/log_marker_x86.o
	$(CC) -nostdlib -static -no-pie -m64 -Wl,-Ttext=0x100000 -Wl,-e,_start -Wl,--build-id=none $^ -o $@

$(BUILD_DIR)/marker_start_arm64.o: $(SMOKE_DIR)/marker_start_arm64.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -ffreestanding -fno-pie -c $< -o $@

$(BUILD_DIR)/marker_main_arm64.o: $(SMOKE_DIR)/marker_main.c include/core/log_marker.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -D__aarch64__ -c $< -o $@

$(BUILD_DIR)/marker_arm64.elf: $(BUILD_DIR)/marker_start_arm64.o $(BUILD_DIR)/marker_main_arm64.o $(BUILD_DIR)/log_marker_arm64.o
	$(AARCH64_CC) -nostdlib -static -no-pie -Wl,-Ttext=0x40000000 -Wl,-e,_start -Wl,--build-id=none $^ -o $@

$(BUILD_DIR)/e1_start_arm64.o: $(SMOKE_DIR)/e1_start_arm64.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -ffreestanding -fno-pie -c $< -o $@

$(BUILD_DIR)/e1_main_arm64.o: $(SMOKE_DIR)/e1_main.c include/core/log_marker.h include/abi/nci.h include/abi/cap.h include/abi/msg.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -D__aarch64__ -c $< -o $@

$(BUILD_DIR)/e1_arm64.elf: $(BUILD_DIR)/e1_start_arm64.o $(BUILD_DIR)/e1_main_arm64.o $(BUILD_DIR)/log_marker_arm64.o
	$(AARCH64_CC) -nostdlib -static -no-pie -Wl,-Ttext=0x40000000 -Wl,-e,_start -Wl,--build-id=none $^ -o $@

.PHONY: mark-e1-x86 mark-e1-arm
mark-e1-x86: $(BUILD_DIR)/marker_x86.elf
	@echo "marker_x86.elf ready @ $(BUILD_DIR)/marker_x86.elf"
	@echo "Run: qemu-system-x86_64 -nographic -serial mon:stdio -debugcon file:$(BUILD_DIR)/port_e9.log -kernel $(BUILD_DIR)/marker_x86.elf"

mark-e1-arm: $(BUILD_DIR)/marker_arm64.elf
	@echo "marker_arm64.elf ready @ $(BUILD_DIR)/marker_arm64.elf"
	@echo "Run: timeout 5s qemu-system-aarch64 -M virt -cpu cortex-a57 -nographic -serial mon:stdio -kernel $(BUILD_DIR)/marker_arm64.elf"

.PHONY: e1-x86 e1-arm
e1-x86: $(BUILD_DIR)/e1_x86.elf
	@echo "e1_x86.elf ready @ $(BUILD_DIR)/e1_x86.elf"
	@echo "Run: timeout 5s qemu-system-x86_64 -nographic -serial mon:stdio -debugcon file:$(BUILD_DIR)/e1_port_e9.log -no-reboot -no-shutdown -kernel $(BUILD_DIR)/e1_x86.elf || true"

e1-arm: $(BUILD_DIR)/e1_arm64.elf
	@echo "e1_arm64.elf ready @ $(BUILD_DIR)/e1_arm64.elf"
	@echo "Run: timeout 5s qemu-system-aarch64 -M virt -cpu cortex-a57 -nographic -serial mon:stdio -no-reboot -no-shutdown -kernel $(BUILD_DIR)/e1_arm64.elf || true"

$(BUILD_DIR)/e3_start_x86.o: $(SMOKE_DIR)/e3_start_x86.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -ffreestanding -fno-pie -m64 -c $< -o $@

$(BUILD_DIR)/pvh_note.o: src/smoke/pvh_note.S
	@mkdir -p $(BUILD_DIR)
	$(CC) -ffreestanding -fno-pie -m64 -c $< -o $@

$(BUILD_DIR)/e3_main_x86.o: $(SMOKE_DIR)/e3_main.c include/core/log_marker.h include/core/parcel.h include/core/cg.h include/core/qos.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -m64 -Iinclude -D__x86_64__ -c $< -o $@

$(BUILD_DIR)/e3_x86.elf: $(BUILD_DIR)/e3_start_x86.o $(BUILD_DIR)/pvh_note.o $(BUILD_DIR)/e3_main_x86.o $(BUILD_DIR)/log_marker_x86.o $(BUILD_DIR)/cg_bare.o $(BUILD_DIR)/qos.o $(PARCEL_OBJ)
	$(CC) -nostdlib -static -no-pie -m64 -Wl,-T,$(LDS_E3_X86) -Wl,-Map,$(BUILD_DIR)/e3_x86.map -Wl,--build-id=none $^ -o $@

$(BUILD_DIR)/e3_start_arm64.o: $(SMOKE_DIR)/e3_start_arm64.S
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -ffreestanding -fno-pie -c $< -o $@

$(BUILD_DIR)/e3_main_arm64.o: $(SMOKE_DIR)/e3_main.c include/core/log_marker.h include/core/parcel.h include/core/cg.h include/core/qos.h
	@mkdir -p $(BUILD_DIR)
	$(AARCH64_CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -D__aarch64__ -c $< -o $@

$(BUILD_DIR)/e3_arm64.elf: $(BUILD_DIR)/e3_start_arm64.o $(BUILD_DIR)/e3_main_arm64.o $(BUILD_DIR)/log_marker_arm64.o $(BUILD_DIR)/cg_arm64.o $(BUILD_DIR)/qos_arm64.o $(BUILD_DIR)/parcel_arm64.o
	$(AARCH64_CC) -nostdlib -static -no-pie -Wl,-T,$(LDS_E3_ARM) -Wl,-Map,$(BUILD_DIR)/e3_arm64.map -Wl,--build-id=none $^ -o $@

.PHONY: e3-x86 e3-arm
e3-x86: $(BUILD_DIR)/e3_x86.elf
	@echo "e3_x86.elf ready @ $(BUILD_DIR)/e3_x86.elf"
	@echo "Run: make run-e3-x86"

.PHONY: run-e3-x86 run-e3-x86-bios
run-e3-x86: $(BUILD_DIR)/e3_x86.elf
	@timeout 5s qemu-system-x86_64 \
	  -machine microvm -cpu max -m 64M -bios none -display none -serial stdio \
	  -no-reboot -no-shutdown \
	  -kernel $(BUILD_DIR)/e3_x86.elf || true

run-e3-x86-bios: $(BUILD_DIR)/e3_x86.elf
	@mkdir -p $(BUILD_DIR)
	@timeout 5s qemu-system-x86_64 \
	  -machine pc -nographic -serial mon:stdio -no-reboot -no-shutdown \
	  -debugcon file:$(BUILD_DIR)/e3_port402.log \
	  -kernel $(BUILD_DIR)/e3_x86.elf || true
	@echo "---- $(BUILD_DIR)/e3_port402.log ----"
	@sed -n '1,120p' $(BUILD_DIR)/e3_port402.log || true

e3-arm: $(BUILD_DIR)/e3_arm64.elf
	@echo "e3_arm64.elf ready @ $(BUILD_DIR)/e3_arm64.elf"
	@echo "Run: timeout 5s qemu-system-aarch64 -M virt -cpu cortex-a57 -nographic -serial mon:stdio -no-reboot -no-shutdown -kernel $(BUILD_DIR)/e3_arm64.elf || true"


$(PARCEL_OBJ): libparcel/parcel.c include/core/parcel.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pie -Iinclude -c libparcel/parcel.c -o $@

$(REPLAY_BIN): tools/replay_trace.c include/core/parcel.h $(PARCEL_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tools/replay_trace.c $(PARCEL_OBJ) -o $@

$(TEST_PARCEL): tests/test_parcel.c include/core/parcel.h $(PARCEL_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_parcel.c $(PARCEL_OBJ) -o $@

$(EMIT_PARCEL): tools/emit_parcel.c include/core/parcel.h $(PARCEL_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tools/emit_parcel.c $(PARCEL_OBJ) -o $@

$(TEST_CG): tests/test_cg.c include/core/cg.h $(BUILD_DIR)/cg.o
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_cg.c $(BUILD_DIR)/cg.o -o $@

$(TEST_QOS): tests/test_qos.c include/core/qos.h $(QOS_HOST_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_qos.c $(QOS_HOST_OBJ) -o $@

$(TEST_SCHED): tests/test_sched.c include/core/scheduler.h include/core/qos.h $(BUILD_DIR)/scheduler.o $(QOS_HOST_OBJ) $(BUILD_DIR)/organelles.o $(ORG_COMMON_OBJS) $(PARCEL_OBJ) $(BUILD_DIR)/channel_graph.o $(CH0_LOG_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_sched.c $(BUILD_DIR)/scheduler.o $(QOS_HOST_OBJ) $(BUILD_DIR)/organelles.o $(ORG_COMMON_OBJS) $(PARCEL_OBJ) $(BUILD_DIR)/channel_graph.o $(CH0_LOG_OBJ) -o $@

$(TEST_LOG_RING): tests/test_log_ring.c include/core/log_ring.h $(BUILD_DIR)/log_ring.o
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_log_ring.c $(BUILD_DIR)/log_ring.o -o $@

.PHONY: parcel replay test-parcel test-cg test-qos test-sched test-logring test-kv
parcel: $(PARCEL_OBJ)
replay: $(REPLAY_BIN)
test-parcel: $(TEST_PARCEL)
	@$(TEST_PARCEL)
test-cg: $(TEST_CG)
	@$(TEST_CG)
test-qos: $(TEST_QOS)
	@$(TEST_QOS)
test-sched: $(TEST_SCHED)
	@$(TEST_SCHED)
test-logring: $(TEST_LOG_RING)
	@cp $(TEST_LOG_RING) $(BUILD_DIR)/tmp/test_log_ring && chmod +x $(BUILD_DIR)/tmp/test_log_ring && TMPDIR="$(BUILD_DIR)/tmp" $(BUILD_DIR)/tmp/test_log_ring
test-kv: $(TEST_KV)
	@cp $(TEST_KV) $(BUILD_DIR)/tmp/test_kv && chmod +x $(BUILD_DIR)/tmp/test_kv && TMPDIR="$(BUILD_DIR)/tmp" $(BUILD_DIR)/tmp/test_kv
test-e0-x86: $(TEST_E0_X86)
	@bash $(TEST_E0_X86)
test-e0-arm: $(TEST_E0_ARM)
	@bash $(TEST_E0_ARM)

.PHONY: smoke run-x86-proof run-arm-proof
run-x86-proof:
	@test -f $(OUT)/cellos-x86_bios.img || (echo "missing $(OUT)/cellos-x86_bios.img — run: make release"; exit 1)
	@mkdir -p $(TMPDIR)
	@timeout 30s scripts/run_x86_qemu.sh $(OUT)/cellos-x86_bios.img | tee $(TMPDIR)/x86.log || true
	@tools/verify_proofs.sh $(TMPDIR)/x86.log

run-arm-proof:
	@test -f $(OUT)/cellos-arm64.elf || (echo "missing $(OUT)/cellos-arm64.elf — run: make release"; exit 1)
	@mkdir -p $(TMPDIR)
	@timeout 30s scripts/run_arm64_qemu.sh $(OUT)/cellos-arm64.elf | tee $(TMPDIR)/arm.log || true
	@tools/verify_proofs.sh $(TMPDIR)/arm.log

$(OUT)/cellos-x86_bios.img: $(DISK_IMG)
	@mkdir -p $(OUT)
	cp $< $@

$(OUT)/cellos-arm64.elf: $(ARM64_KERNEL_ELF)
	@mkdir -p $(OUT)
	cp $< $@

.PHONY: run-e0-x86 run-e0-arm
run-e0-x86: $(OUT)/cellos-x86_bios.img
	$(MAKE) run-x86-proof

run-e0-arm: $(OUT)/cellos-arm64.elf
	$(MAKE) run-arm-proof

smoke: check-freeze
	@$(MAKE) -s test-all
	@$(MAKE) -s run-x86-proof

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) out_det_a out_det_b

include e4.mk

include e5.mk

.PHONY: dev-all
dev-all: e3-x86 e3-arm e4 e5

include e6.mk

include e7.mk

include e8.mk

include e9.mk


.PHONY: all test-all channel scheduler test-abi
all: check-abi parcel channel scheduler phenotypes organelles irq update obs security

test-all: test-abi test-parcel test-cg test-sched test-logring test-phenotype test-org test-irq test-update test-obs test-sec


channel: $(BUILD_DIR)/cg.o $(BUILD_DIR)/cg_bare.o $(BUILD_DIR)/cg_arm64.o

scheduler: $(BUILD_DIR)/scheduler.o

test-abi: check-abi

.PHONY: pack-disk
pack-disk: $(DISK_IMG)

$(CGF_BIN): tools/gen_cgf.py FORCE
	@mkdir -p $(BUILD_DIR)
	@python3 tools/gen_cgf.py $(GEN_CGF_FLAGS) $@

ifeq ($(CFG_ENABLE_CGF_VERIFY),1)
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(CGF_BIN) scripts/pack_disk.py
	@mkdir -p $(BUILD_DIR)
	@python3 scripts/pack_disk.py $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(CGF_BIN) $@
	@echo "#E0 pack-disk ok (own boot chain + cgf)"
else
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) scripts/pack_disk.py
	@mkdir -p $(BUILD_DIR)
	@python3 scripts/pack_disk.py $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $@
	@echo "#E0 pack-disk ok (own boot chain)"
endif

.PHONY: e12-cgf-verify
e12-cgf-verify:
	@echo "[E12A] x86 CGF verify (flag)"
	@mkdir -p $(BUILD_DIR)/tmp
	@LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1700000000 TMPDIR="$(BUILD_DIR)/tmp" CFG_ENABLE_CGF_VERIFY=1 $(MAKE) -s pack-disk
	@bash -c 'timeout 10s env CFG_ENABLE_CGF_VERIFY=1 TMPDIR="$(BUILD_DIR)/tmp" scripts/run_x86_qemu.sh $(DISK_IMG) | tee $(BUILD_DIR)/pf12_x86.log; status=$${PIPESTATUS[0]}; if [ $$status -ne 0 ] && [ $$status -ne 124 ]; then exit $$status; fi'

.PHONY: e12-cgf-verify-bad
e12-cgf-verify-bad:
	@echo "[E12A] x86 CGF verify (corrupt CRC expect #A!)"
	@mkdir -p $(BUILD_DIR)/tmp
	@LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1700000000 TMPDIR="$(BUILD_DIR)/tmp" CFG_ENABLE_CGF_VERIFY=1 GEN_CGF_FLAGS=--corrupt-crc $(MAKE) -s pack-disk
	@bash -c 'timeout 6s env CFG_ENABLE_CGF_VERIFY=1 TMPDIR="$(BUILD_DIR)/tmp" scripts/run_x86_qemu.sh $(DISK_IMG) | tee $(BUILD_DIR)/pf12_x86_bad.log; status=$${PIPESTATUS[0]}; if [ $$status -ne 0 ] && [ $$status -ne 124 ]; then exit $$status; fi'

.PHONY: e12-arm-irq
e12-arm-irq:
	@echo "[E12B] ARM IRQ timer (flag)"
	@mkdir -p $(BUILD_DIR)/tmp
	@rm -rf $(BUILD_DIR)/kernel_arm64 $(ARM64_KERNEL_ELF) $(ARM64_START_OBJ) $(ARM64_KERNEL_MAIN_OBJ) $(ARM64_KERNEL_PROOFS_OBJ) $(ARM64_KERNEL_RUNTIME_OBJ) $(ARM64_KERNEL_NOTE_OBJ) $(PF12_VECTOR_OBJ)
	@LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1700000000 TMPDIR="$(BUILD_DIR)/tmp" CFG_ENABLE_IRQ_TIMER=1 $(MAKE) -s arm64-smoke
	@bash -c 'timeout 12s env CFG_ENABLE_IRQ_TIMER=1 TMPDIR="$(BUILD_DIR)/tmp" scripts/run_arm64_qemu.sh $(ARM64_KERNEL_ELF) | tee $(BUILD_DIR)/pf12_arm.log; status=$${PIPESTATUS[0]}; if [ $$status -ne 0 ] && [ $$status -ne 124 ]; then exit $$status; fi'

.PHONY: e12-logring
e12-logring:
	@echo "[E12C] log ring (flag + host test)"
	@mkdir -p $(BUILD_DIR)/tmp
	@TMPDIR="$(BUILD_DIR)/tmp" $(MAKE) -s test-logring
	@rm -rf $(BUILD_DIR)/kernel_arm64 $(ARM64_KERNEL_ELF) $(ARM64_START_OBJ) $(ARM64_KERNEL_MAIN_OBJ) $(ARM64_KERNEL_PROOFS_OBJ) $(ARM64_KERNEL_RUNTIME_OBJ) $(ARM64_KERNEL_NOTE_OBJ) $(PF12_VECTOR_OBJ)
	@LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1700000000 TMPDIR="$(BUILD_DIR)/tmp" CFG_ENABLE_IRQ_TIMER=1 CFG_ENABLE_NONBLOCK_LOG=1 $(MAKE) -s arm64-smoke
	@bash -c 'timeout 6s env CFG_ENABLE_IRQ_TIMER=1 CFG_ENABLE_NONBLOCK_LOG=1 TMPDIR="$(BUILD_DIR)/tmp" scripts/run_arm64_qemu.sh $(ARM64_KERNEL_ELF) | tee $(BUILD_DIR)/pf12_logring.log; status=$${PIPESTATUS[0]}; if [ $$status -ne 0 ] && [ $$status -ne 124 ]; then exit $$status; fi'

.PHONY: e12-kv
e12-kv:
	@echo "[E12D] organelle PKV (flag + host test)"
	@mkdir -p $(BUILD_DIR)/tmp
	@TMPDIR="$(BUILD_DIR)/tmp" $(MAKE) -s test-kv
	@rm -rf $(BUILD_DIR)/kernel_arm64 $(ARM64_KERNEL_ELF) $(ARM64_START_OBJ) $(ARM64_KERNEL_MAIN_OBJ) $(ARM64_KERNEL_PROOFS_OBJ) $(ARM64_KERNEL_RUNTIME_OBJ) $(ARM64_KERNEL_NOTE_OBJ) $(PF12_VECTOR_OBJ)
	@LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1700000000 TMPDIR="$(BUILD_DIR)/tmp" CFG_ENABLE_IRQ_TIMER=1 CFG_ENABLE_ORG_PKV=1 $(MAKE) -s arm64-smoke
	@bash -c 'timeout 6s env CFG_ENABLE_IRQ_TIMER=1 CFG_ENABLE_ORG_PKV=1 TMPDIR="$(BUILD_DIR)/tmp" scripts/run_arm64_qemu.sh $(ARM64_KERNEL_ELF) | tee $(BUILD_DIR)/pf12_kv.log; status=$${PIPESTATUS[0]}; if [ $$status -ne 0 ] && [ $$status -ne 124 ]; then exit $$status; fi'

.PHONY: bios-smoke run-bios-smoke
bios-smoke: pack-disk

run-bios-smoke: bios-smoke
	@echo "[QEMU] x86 BIOS smoke (debugcon→stdio)"
	@timeout 8s qemu-system-x86_64 \
	  -machine pc -cpu max -m 64M -nographic \
	  -no-reboot -no-shutdown -monitor none -serial none \
	  -chardev stdio,id=dbg \
	  -device isa-debugcon,iobase=0xE9,chardev=dbg \
	  -drive file=$(DISK_IMG),format=raw || true

.PHONY: arm64-smoke run-arm64-smoke
arm64-smoke: $(ARM64_KERNEL_ELF)

run-arm64-smoke: arm64-smoke
	@echo "[QEMU] ARM64 virt smoke (PL011→stdio)"
	@timeout 8s qemu-system-aarch64 \
	  -M virt -cpu cortex-a57 -m 64M -nographic \
	  -no-reboot -no-shutdown -monitor none \
	  -serial stdio \
	  -kernel $(ARM64_KERNEL_ELF) || true

include e10.mk

release: check-freeze
