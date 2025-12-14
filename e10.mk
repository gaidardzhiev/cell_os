OUT ?= ../out
PHO := $(OUT)/phenos
SOURCE_DATE_EPOCH ?= 1700000000

.PHONY: release dist run-x86 run-arm64
release:
	@mkdir -p $(OUT) $(PHO)
	@env LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH) bash scripts/build_release.sh

dist: release
	@cd $(OUT) && tar --sort=name --owner=0 --group=0 --numeric-owner \
	  --mtime="@$(SOURCE_DATE_EPOCH)" -cf cellos-$(shell cat ../cell_os/VERSION).tar \
	  MANIFEST.txt BUILDINFO.json *.img *.elf phenos/*.pbin && \
	  gzip -n -9 cellos-$(shell cat ../cell_os/VERSION).tar

run-x86:
	@bash scripts/run_x86_qemu.sh $(OUT)/cellos-x86_bios.img
run-arm64:
	@bash scripts/run_arm64_qemu.sh $(OUT)/cellos-arm64.elf
