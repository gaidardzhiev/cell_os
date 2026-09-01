# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cell OS unified substrate, Cortex, CellFS, CellExec, and proof build graph
CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy
BUILD_DIR ?= build
CORTEX_MODEL ?= models/cortex_demo.cwm
CELLFS_IMAGE ?= state/cellfs.img
CELLFS_SECTORS ?= 1024
DISK_IMG := $(BUILD_DIR)/disk.img
STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
HOST_TEST := $(BUILD_DIR)/test_cortex_host
CELL_PROGRAM_SOURCES := programs/hello.c programs/observe.c
CELL_PROGRAM_IMAGES := $(patsubst programs/%.c,$(BUILD_DIR)/programs/%.cellx,$(CELL_PROGRAM_SOURCES))
HOST_CC := $(BUILD_DIR)/cc

KERNEL_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding \
	-fno-pic -fno-pie -fno-plt -fno-stack-protector -fno-builtin \
	-fno-asynchronous-unwind-tables -fno-exceptions -mcmodel=large -m64 \
	-msse2 -mno-red-zone -Iinclude -Isrc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

KERNEL_SRCS := \
	src/kernel/kernel64_main.c \
	src/kernel/runtime.c \
	src/kernel/cortex_boot.c \
	src/core/mem_arena.c \
	src/core/cellfs.c \
	src/core/vfs.c \
	src/core/capability.c \
	src/core/cellexec.c \
	src/core/task.c \
	src/core/shell.c \
	src/core/cc.c \
	src/cortex/cwm.c \
	src/cortex/cortex.c \
	src/cortex/session.c \
	src/drivers/x86/ata_pio.c
KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/src/kernel/kernel_entry.o

.PHONY: all clean reset-cellfs cortex-model cell-programs install-cell-programs test-cortex-host test-capability test-cellfs test-cellfs-image test-cellfs-tools test-vfs test-cellexec test-task test-cc test-shell test-exec-tools test-program-image test-cortex-session test-caploop cortex-x86 run-cortex-x86 check-tools
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

$(BUILD_DIR)/src/core/cc.o: src/core/cc.c include/core/cc.h include/core/cellexec.h include/core/capability.h
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -O2,$(KERNEL_CFLAGS)) -Os -c $< -o $@

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


$(CELLFS_IMAGE): scripts/mkcellfs.py
	@mkdir -p $(dir $@)
	python3 scripts/mkcellfs.py $@ --sectors $(CELLFS_SECTORS)

reset-cellfs:
	rm -f $(CELLFS_IMAGE)
	@$(MAKE) $(CELLFS_IMAGE)


$(HOST_CC): tools/cc.c src/core/cc.c src/core/cellexec.c include/core/cc.h include/core/cellexec.h include/core/capability.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tools/cc.c src/core/cc.c src/core/cellexec.c -o $@

$(BUILD_DIR)/programs/%.cellx: programs/%.c $(HOST_CC)
	@mkdir -p $(dir $@)
	$(HOST_CC) $< -o $@

cell-programs: $(CELL_PROGRAM_IMAGES)

install-cell-programs: cell-programs $(CELLFS_IMAGE) scripts/cellfs_install.py
	@for p in $(CELL_PROGRAM_IMAGES); do \
		name=$$(basename "$$p" .cellx); \
		python3 scripts/cellfs_install.py $(CELLFS_IMAGE) "$$p" --name "$$name"; \
	done

$(HOST_TEST): src/cortex/cwm.c src/cortex/cortex.c tests/test_cortex_host.c include/cortex/cwm.h include/cortex/cortex.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/cortex/cwm.c src/cortex/cortex.c tests/test_cortex_host.c -o $@

$(BUILD_DIR)/test_capability: src/core/capability.c src/core/vfs.c src/core/cellfs.c tests/test_capability.c include/core/capability.h include/core/vfs.h include/core/cellfs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/capability.c src/core/vfs.c src/core/cellfs.c tests/test_capability.c -o $@

test-capability: $(BUILD_DIR)/test_capability
	@$(BUILD_DIR)/test_capability

$(BUILD_DIR)/test_cellfs: src/core/cellfs.c tests/test_cellfs.c include/core/cellfs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cellfs.c tests/test_cellfs.c -o $@

test-cellfs: $(BUILD_DIR)/test_cellfs
	@$(BUILD_DIR)/test_cellfs

$(BUILD_DIR)/test_cellfs_image: src/core/cellfs.c tests/test_cellfs_image.c include/core/cellfs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cellfs.c tests/test_cellfs_image.c -o $@

test-cellfs-image: $(BUILD_DIR)/test_cellfs_image $(CELLFS_IMAGE)
	@$(BUILD_DIR)/test_cellfs_image $(CELLFS_IMAGE)

test-cellfs-tools: scripts/mkcellfs.py scripts/sync_cellfs.py scripts/pack_disk.py tests/test_cellfs_tools.py
	@python3 tests/test_cellfs_tools.py

$(BUILD_DIR)/test_vfs: src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_vfs.c include/core/cellfs.h include/core/vfs.h include/core/capability.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_vfs.c -o $@

test-vfs: $(BUILD_DIR)/test_vfs
	@$(BUILD_DIR)/test_vfs


$(BUILD_DIR)/test_cellexec: src/core/cellexec.c tests/test_cellexec.c include/core/cellexec.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cellexec.c tests/test_cellexec.c -o $@

test-cellexec: $(BUILD_DIR)/test_cellexec
	@$(BUILD_DIR)/test_cellexec

$(BUILD_DIR)/test_task: src/core/cellexec.c src/core/task.c src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_task.c include/core/cellexec.h include/core/task.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cellexec.c src/core/task.c src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_task.c -o $@

test-task: $(BUILD_DIR)/test_task
	@$(BUILD_DIR)/test_task

$(BUILD_DIR)/test_cc: src/core/cc.c src/core/cellexec.c tests/test_cc.c include/core/cc.h include/core/cellexec.h include/core/capability.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cc.c src/core/cellexec.c tests/test_cc.c -o $@

test-cc: $(BUILD_DIR)/test_cc
	@$(BUILD_DIR)/test_cc

$(BUILD_DIR)/test_shell: src/core/shell.c src/core/cc.c src/core/cellexec.c src/core/task.c src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_shell.c include/core/shell.h include/core/cc.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/shell.c src/core/cc.c src/core/cellexec.c src/core/task.c src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_shell.c -o $@

test-shell: $(BUILD_DIR)/test_shell
	@$(BUILD_DIR)/test_shell

test-exec-tools: tools/cellasm.py scripts/cellfs_install.py scripts/mkcellfs.py tests/test_exec_tools.py
	@python3 tests/test_exec_tools.py

$(BUILD_DIR)/test_program_image: src/core/cellexec.c src/core/task.c src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_program_image.c
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/core/cellexec.c src/core/task.c src/core/cellfs.c src/core/vfs.c src/core/capability.c tests/test_program_image.c -o $@

test-program-image: $(BUILD_DIR)/test_program_image install-cell-programs
	@$(BUILD_DIR)/test_program_image $(CELLFS_IMAGE)

$(BUILD_DIR)/test_cortex_session: src/cortex/cwm.c src/cortex/cortex.c src/cortex/session.c src/core/capability.c src/core/vfs.c src/core/cellfs.c src/core/cellexec.c src/core/task.c src/core/shell.c src/core/cc.c tests/test_cortex_session.c
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/cortex/cwm.c src/cortex/cortex.c src/cortex/session.c src/core/capability.c src/core/vfs.c src/core/cellfs.c src/core/cellexec.c src/core/task.c src/core/shell.c src/core/cc.c tests/test_cortex_session.c -o $@

test-cortex-session: $(BUILD_DIR)/test_cortex_session cortex-model
	@$(BUILD_DIR)/test_cortex_session $(CORTEX_MODEL)

test-cortex-host: $(HOST_TEST) cortex-model
	@out="$$( $(HOST_TEST) $(CORTEX_MODEL) 'cell> hello' )"; printf '%s\n' "$$out"; \
		printf '%s\n' "$$out" | grep -q 'Hello. I am Cell Cortex' || { echo '#CORTEX host inference FAIL'; exit 1; }
	@echo '#CORTEX host inference PASS'

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(CORTEX_MODEL) $(CELLFS_IMAGE) install-cell-programs scripts/pack_disk.py
	@mkdir -p $(BUILD_DIR)
	python3 scripts/pack_disk.py $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $@ --model $(CORTEX_MODEL) --cellfs $(CELLFS_IMAGE)

test-caploop: test-capability test-cellfs test-cellfs-image test-cellfs-tools test-vfs test-cellexec test-task test-cc test-shell test-exec-tools test-program-image test-cortex-host test-cortex-session

cortex-x86: check-tools test-caploop $(DISK_IMG)
	@echo '#CORTEX x86 image ready:' $(DISK_IMG)

run-cortex-x86: cortex-x86
	@command -v qemu-system-x86_64 >/dev/null || { echo 'missing: qemu-system-x86_64'; exit 1; }
	@scripts/run_cortex_x86.sh $(DISK_IMG) $(CELLFS_IMAGE)

clean:
	rm -rf $(BUILD_DIR) out_det_a out_det_b

# =============================================================================
# Legacy proof and subsystem graph
#
# Consolidated into the top-level Makefile. The historical E3-E9 proof
# targets remain available, but milestone-specific e*.mk files are no longer
# part of the build system.
# =============================================================================

AARCH64_CC ?= aarch64-linux-gnu-gcc
OUT ?= ../out
TMPDIR ?= /tmp

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
	@test -f $(OUT)/cellos-x86_bios.img || (echo "missing $(OUT)/cellos-x86_bios.img, run: make release"; exit 1)
	@mkdir -p $(TMPDIR)
	@timeout 30s scripts/run_x86_qemu.sh $(OUT)/cellos-x86_bios.img | tee $(TMPDIR)/x86.log || true
	@tools/verify_proofs.sh $(TMPDIR)/x86.log

run-arm-proof:
	@test -f $(OUT)/cellos-arm64.elf || (echo "missing $(OUT)/cellos-arm64.elf, run: make release"; exit 1)
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

# =============================================================================
# E4 proof/subsystem rules
# =============================================================================
PY ?= python3

PHENO_SRC := phenotypes/x86_bios.minimal.yaml phenotypes/arm64.virt.pl011.yaml
PHENO_BIN := $(patsubst phenotypes/%.yaml,build/%.pbin,$(PHENO_SRC))

build/%.pbin: phenotypes/%.yaml tools/phenotype_compile.py schemas/phenotype.schema.yaml
	@mkdir -p $(dir $@)
	$(PY) tools/phenotype_compile.py $< $@

.PHONY: phenotypes
phenotypes: $(PHENO_BIN)

BUILD_TEST_PHENO := build/test_phenotype_loader
$(BUILD_TEST_PHENO): tests/test_phenotype_loader.c src/core/phenotype_loader.c include/core/phenotype.h include/core/channel.h \
	$(BUILD_DIR)/scheduler.o $(QOS_HOST_OBJ) $(BUILD_DIR)/organelles.o $(ORG_COMMON_OBJS) $(PARCEL_OBJ) $(BUILD_DIR)/channel_graph.o $(CH0_LOG_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -Iinclude/core \
		tests/test_phenotype_loader.c src/core/phenotype_loader.c \
		$(BUILD_DIR)/scheduler.o $(QOS_HOST_OBJ) $(BUILD_DIR)/organelles.o \
		$(ORG_COMMON_OBJS) $(PARCEL_OBJ) $(BUILD_DIR)/channel_graph.o $(CH0_LOG_OBJ) -o $@

.PHONY: test-phenotype
test-phenotype: phenotypes $(BUILD_TEST_PHENO)
	@$(BUILD_TEST_PHENO)

E4_ELF_X86 ?= build/e3_x86.elf
E4_ELF_ARM ?= build/e3_arm64.elf

.PHONY: e4 e4-abi-check e4-nopie-check e4-sym-check e4-sections-check e4-size-lints \
	e4-hash-check e4-determinism e4-abi-compilecheck
e4: phenotypes test-phenotype check-abi e4-hash-check e4-abi-check e4-nopie-check e4-sym-check \
	e4-sections-check e4-size-lints e4-determinism e4-abi-compilecheck
	@echo "#E4 interop ok"

e4-hash-check: phenotypes
	@echo "[E4] sha256 verify of .pbin"
	@cd build && set -e; \
		if ! find . -type f -name "*.sha256" -print -quit | grep -q .; then \
			echo "No .sha256 files found" >&2; exit 1; \
		fi; \
		find . -type f -name "*.sha256" -print0 | xargs -0 -n1 sha256sum -c

e4-abi-check:
	@echo "[E4] ABI/machine headers"
	@test -f "$(E4_ELF_X86)" && test -f "$(E4_ELF_ARM)"
	readelf -h "$(E4_ELF_X86)" | grep -q "Machine:.*Advanced Micro Devices X86-64"
	readelf -h "$(E4_ELF_X86)" | grep -q "Type:.*EXEC\\|REL"
	readelf -h "$(E4_ELF_ARM)" | grep -q "Machine:.*AArch64"
	readelf -h "$(E4_ELF_ARM)" | grep -q "Type:.*EXEC\\|REL"

e4-nopie-check:
	@echo "[E4] no PIE/DSO/PLT"
	! readelf -d "$(E4_ELF_X86)" | grep -qi '\(NEEDED\|RUNPATH\|RPATH\)'
	! readelf -r "$(E4_ELF_X86)" | grep -qi 'R_X86_64_PLT32'
	! readelf -d "$(E4_ELF_ARM)" | grep -qi '\(NEEDED\|RUNPATH\|RPATH\)'

e4-sym-check:
	@echo "[E4] symbol hygiene"
	! nm -u "$(E4_ELF_X86)" | grep -E '__stack_chk|@GLIBC' || (echo "Unexpected host dependency in x86_64"; exit 1)
	! nm -u "$(E4_ELF_ARM)" | grep -E '__stack_chk|@GLIBC' || (echo "Unexpected host dependency in arm64"; exit 1)
	@if nm "$(E4_ELF_X86)" | awk '{print $$3}' | grep -E '^_' | grep -v '^_start$$' >/dev/null; then \
	  echo "Leading '_' symbols detected in $(E4_ELF_X86)"; exit 1; fi

e4-sections-check:
	@echo "[E4] section sanity"
	readelf -S "$(E4_ELF_X86)" | grep -q '\.text'
	readelf -S "$(E4_ELF_X86)" | grep -q '\.rodata'
	readelf -S "$(E4_ELF_X86)" | grep -q '\.bss'
	readelf -S "$(E4_ELF_ARM)" | grep -q '\.text'
	readelf -S "$(E4_ELF_ARM)" | grep -q '\.rodata'
	readelf -S "$(E4_ELF_ARM)" | grep -q '\.bss'

e4-size-lints:
	@echo "[E4] size/reloc lints"
	size "$(E4_ELF_X86)" || true
	size "$(E4_ELF_ARM)" || true
	! objdump -r "$(E4_ELF_X86)" | grep -E '\.text.*R_X86_64_32([^S]|$$)'

e4-determinism:
	@echo "[E4] determinism check (phenotypes)"
	rm -rf out_det_a out_det_b
	mkdir -p out_det_a out_det_b
	@(command -v git >/dev/null 2>&1 && git rev-parse --short HEAD 2>/dev/null || echo "no-git") > out_det_a/build_id.txt
	cp out_det_a/build_id.txt out_det_b/build_id.txt
	find build -maxdepth 1 -type f -name "*.pbin" -delete 2>/dev/null || true
	find build -maxdepth 1 -type f -name "*.sha256" -delete 2>/dev/null || true
	$(MAKE) -s phenotypes
	@set -e; found=0; for f in build/*.pbin build/*.sha256; do \
		if [ -f "$$f" ]; then cp "$$f" out_det_a/; found=1; fi; \
	done; if [ $$found -eq 0 ]; then echo "No phenotypes emitted"; exit 1; fi
	find build -maxdepth 1 -type f -name "*.pbin" -delete 2>/dev/null || true
	find build -maxdepth 1 -type f -name "*.sha256" -delete 2>/dev/null || true
	$(MAKE) -s phenotypes
	@set -e; found=0; for f in build/*.pbin build/*.sha256; do \
		if [ -f "$$f" ]; then cp "$$f" out_det_b/; found=1; fi; \
	done; if [ $$found -eq 0 ]; then echo "No phenotypes emitted"; exit 1; fi
	@echo "[E4] comparing blobs..."
	diff -qNr out_det_a out_det_b
	@echo "[E4] determinism ok"

ABI_COMPILE_TMP ?= $(BUILD_DIR)/abi_check.compile.o
e4-abi-compilecheck:
	@echo "[E4] compile-time ABI static assertions"
	@mkdir -p $(dir $(ABI_COMPILE_TMP))
	$(CC) -std=c11 -Wall -Wextra -Werror -ffreestanding -Iinclude -c src/core/abi_check.c -o $(ABI_COMPILE_TMP)
	rm -f $(ABI_COMPILE_TMP)

.PHONY: pbindump
pbindump:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make pbindump FILE=build/foo.pbin"; exit 1; \
	fi
	$(PY) tools/pbindump.py "$(FILE)"

# =============================================================================
# E5 proof/subsystem rules
# =============================================================================
# E5: Org Pack v1

ORG_SRCS ?= $(ORG_COMMON_SRCS)
ORG_HDRS ?= $(ORG_COMMON_HDRS)
ORG_OBJS ?= $(ORG_COMMON_OBJS)
MITO_OBJ := $(BUILD_DIR)/genes/mito_energy.o
GOLGI_OBJ := $(BUILD_DIR)/genes/golgi_route.o
LYSO_OBJ := $(BUILD_DIR)/genes/lyso_cleanup.o
PEROXI_OBJ := $(BUILD_DIR)/genes/peroxi_sanitize.o

$(BUILD_DIR)/abi_e5_check.o: src/core/abi_e5_check.c $(ORG_HDRS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Iinclude -c $< -o $@

.PHONY: organelles
organelles: $(ORG_OBJS) $(BUILD_DIR)/organelles.o $(BUILD_DIR)/channel_graph.o $(BUILD_DIR)/abi_e5_check.o

TEST_ORG := build/test_mito build/test_golgi build/test_lyso build/test_peroxi

build/test_mito: tests/test_mito.c $(ORG_HDRS) $(MITO_OBJ) $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_mito.c $(MITO_OBJ) $(PARCEL_OBJ) -o $@

build/test_golgi: tests/test_golgi.c $(ORG_HDRS) $(GOLGI_OBJ) $(BUILD_DIR)/channel_graph.o include/core/channel.h $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_golgi.c $(GOLGI_OBJ) $(BUILD_DIR)/channel_graph.o $(PARCEL_OBJ) -o $@

build/test_lyso: tests/test_lyso.c $(ORG_HDRS) $(LYSO_OBJ) $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_lyso.c $(LYSO_OBJ) $(PARCEL_OBJ) -o $@

build/test_peroxi: tests/test_peroxi.c $(ORG_HDRS) $(PEROXI_OBJ) $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_peroxi.c $(PEROXI_OBJ) $(PARCEL_OBJ) -o $@

.PHONY: test-org
test-org: $(TEST_ORG)
	@set -e; for t in $(TEST_ORG); do $$t; done; echo "#E5 ORG ok"

.PHONY: e5
e5: organelles test-org

# =============================================================================
# E6 proof/subsystem rules
# =============================================================================
# E6: Interrupt > Event Bridge

IRQ_OBJS := build/irq_bridge.o build/events.o
TEST_IRQ := build/test_irq_flood build/test_irq_order

build/irq_bridge.o: src/core/irq_bridge.c include/core/irq_bridge.h include/core/events.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/events.o: src/core/events.c include/core/events.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/test_irq_flood: tests/test_irq_flood.c $(IRQ_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_irq_flood.c $(IRQ_OBJS) -o $@

build/test_irq_order: tests/test_irq_order.c $(IRQ_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_irq_order.c $(IRQ_OBJS) -o $@

.PHONY: irq test-irq
irq: $(IRQ_OBJS)

test-irq: $(TEST_IRQ)
	@set -e; for t in $(TEST_IRQ); do $$t; done; echo "#E6 EVT bridge ok"

# =============================================================================
# E7 proof/subsystem rules
# =============================================================================
# E7: Update/Rollback v2 + Migration Gene

UPDATE_OBJ := build/update.o
TEST_UPDATE := build/test_update
TEST_MIGRATE := build/test_migrate

build/update.o: src/core/update.c include/core/update.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/test_update: tests/test_update.c src/core/update.c include/core/update.h tools/mock_storage.h $(HASH_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -I. tests/test_update.c src/core/update.c $(HASH_OBJS) -o $@

build/test_migrate: tests/test_migrate.c src/genes/migrate.c include/gene/migrate.h include/gene/organelle.h include/core/parcel.h include/abi/msg.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -I. tests/test_migrate.c src/genes/migrate.c libparcel/parcel.c -o $@

.PHONY: update test-update
update: $(UPDATE_OBJ)

test-update: build/test_update build/test_migrate
	@build/test_update
	@build/test_migrate
	@echo "#E7 UPDATE ready"

HASH_OBJS := build/hash_stub.o
ifdef BUILD_STRICT_CRYPTO
CFLAGS += -DBUILD_STRICT_CRYPTO=1
HASH_OBJS += build/hash_sha256.o
endif

build/hash_stub.o: src/core/hash_stub.c include/core/hash.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/hash_sha256.o: src/core/hash_sha256.c include/core/hash.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/update.o: $(HASH_OBJS)
build/test_update: $(HASH_OBJS)

# =============================================================================
# E8 proof/subsystem rules
# =============================================================================
# E8 Observability v2 (core) ???

OBS_OBJS := build/obs.o build/obs_trace.o

build/obs.o: src/core/obs.c include/core/obs.h include/abi/msg.h include/core/parcel.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/obs.c -o $@

build/obs_trace.o: src/core/obs_trace.c include/core/obs_trace.h include/core/obs.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/obs_trace.c -o $@

.PHONY: obs
obs: $(OBS_OBJS)

REPLAY_BIN := build/obs_replay
INDEX_BIN := build/obs_index
TEST_OBS_EQ := build/test_obs_replay_equiv
TEST_OBS_FR := build/test_obs_frames

$(REPLAY_BIN): tools/obs_replay.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c \
	include/core/obs.h include/core/obs_trace.h include/core/parcel.h include/abi/msg.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tools/obs_replay.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c -o $@

$(INDEX_BIN): tools/obs_index.c src/core/obs_trace.c include/core/obs_trace.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tools/obs_index.c src/core/obs_trace.c -o $@

$(TEST_OBS_EQ): tests/test_obs_replay_equiv.c src/core/obs.c libparcel/parcel.c \
	include/core/obs.h include/core/parcel.h include/abi/msg.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_obs_replay_equiv.c src/core/obs.c libparcel/parcel.c -o $@

$(TEST_OBS_FR): tests/test_obs_frames.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c \
	include/core/obs.h include/core/obs_trace.h include/core/parcel.h include/abi/msg.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_obs_frames.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c -o $@

.PHONY: replay test-obs
replay: $(REPLAY_BIN)

.PHONY: index
index: $(INDEX_BIN)

test-obs: $(TEST_OBS_EQ) $(TEST_OBS_FR) $(REPLAY_BIN)
	@./$(TEST_OBS_EQ)
	@./$(TEST_OBS_FR) | ./$(REPLAY_BIN) >/dev/null 2>/dev/null
	@echo "#E8 OBS v2 replay=ok"

# =============================================================================
# E9 proof/subsystem rules
# =============================================================================
#E9 Security (MAC + Trust + Fuzz) ???

SEC_OBJS := $(BUILD_DIR)/crypto.o $(BUILD_DIR)/trust.o
TEST_MAC := $(BUILD_DIR)/test_mac
TEST_TRUST := $(BUILD_DIR)/test_trust
FUZZ := $(BUILD_DIR)/fuzz_parsers

$(BUILD_DIR)/crypto.o: src/core/crypto.c include/core/crypto.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(BUILD_DIR)/trust.o: src/core/trust.c include/core/trust.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(TEST_MAC): tests/test_mac.c include/core/crypto.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $< -o $@

$(TEST_TRUST): tests/test_trust.c include/core/trust.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $< -o $@

$(FUZZ): tests/fuzz_parsers.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c \
		 include/core/obs.h include/core/obs_trace.h include/core/parcel.h include/abi/msg.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $^ -o $@

.PHONY: security test-sec
security: $(SEC_OBJS)

test-sec: $(TEST_MAC) $(TEST_TRUST) $(FUZZ)
	@$(TEST_MAC)
	@$(TEST_TRUST)
	@$(FUZZ)
	@echo "#E9 SEC mac+roots"

# =============================================================================
# Legacy aggregate targets
# =============================================================================

.PHONY: dev-all legacy-all test-all channel scheduler test-abi

dev-all: e3-x86 e3-arm e4 e5

legacy-all: check-abi parcel channel scheduler phenotypes organelles irq update obs security

test-all: test-abi test-parcel test-cg test-sched test-logring \
	test-phenotype test-org test-irq test-update test-obs test-sec

channel: $(BUILD_DIR)/cg.o $(BUILD_DIR)/cg_bare.o $(BUILD_DIR)/cg_arm64.o

scheduler: $(BUILD_DIR)/scheduler.o

test-abi: check-abi

# Compatibility names required by the historical release tooling.
.PHONY: kernel pack-disk
kernel: $(KERNEL_BIN)
pack-disk: $(DISK_IMG)

# =============================================================================
# Release
# =============================================================================

SOURCE_DATE_EPOCH ?= 1700000000
RELEASE_VERSION := $(shell cat VERSION)

.PHONY: release dist run-x86

release: check-freeze test-caploop cortex-x86 phenotypes
	@mkdir -p $(OUT)/phenos
	@cp -f $(DISK_IMG) $(OUT)/cellos-x86_bios.img
	@cp -f $(BUILD_DIR)/*.pbin $(OUT)/phenos/ 2>/dev/null || true
	@printf '%s\n' \
		'{' \
		'  "version": "$(RELEASE_VERSION)",' \
		'  "source_date_epoch": $(SOURCE_DATE_EPOCH)' \
		'}' > $(OUT)/BUILDINFO.json
	@cd $(OUT) && find . -type f ! -name MANIFEST.txt -print0 | \
		sort -z | xargs -0 sha256sum > MANIFEST.txt
	@echo "#RELEASE ok version=$(RELEASE_VERSION)"

dist: release
	@cd $(OUT) && tar \
		--sort=name \
		--owner=0 \
		--group=0 \
		--numeric-owner \
		--mtime="@$(SOURCE_DATE_EPOCH)" \
		-cf cellos-$(RELEASE_VERSION).tar \
		MANIFEST.txt BUILDINFO.json cellos-x86_bios.img phenos
	@gzip -n -9 -f $(OUT)/cellos-$(RELEASE_VERSION).tar
	@echo "#DIST $(OUT)/cellos-$(RELEASE_VERSION).tar.gz"

run-x86: cortex-x86
	@scripts/run_cortex_x86.sh $(DISK_IMG) $(CELLFS_IMAGE)

# ARM64 Cortex support is not claimed by the current reference kernel.
# Do not silently resurrect the old ARM release recipe until an ARM Cortex
# backend exists again.
