# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
# Licensed under the MIT License. See LICENSE in project root.

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
