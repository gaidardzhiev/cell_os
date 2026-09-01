#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="${1:-$ROOT/build/disk.img}"
CELLFS="${2:-$ROOT/state/cellfs.img}"
RAM="${CELLOS_RAM:-512M}"
DBG="${CELLOS_DEBUG_LOG:-$ROOT/build/cortex-debug.log}"
if [[ ! -f "$IMG" ]]; then
	make -C "$ROOT" cortex-x86
fi
mkdir -p "$(dirname "$DBG")"
set +e
qemu-system-x86_64 \
	-machine pc -cpu max -m "$RAM" \
	-display none -monitor none -no-reboot -no-shutdown \
	-serial stdio \
	-chardev file,id=dbg,path="$DBG" \
	-device isa-debugcon,iobase=0xE9,chardev=dbg \
	-drive file="${IMG}",format=raw,if=ide
status=$?
set -e
if [[ -f "$IMG" && -f "$CELLFS" ]]; then
	python3 "$ROOT/scripts/sync_cellfs.py" "$IMG" "$CELLFS"
fi
exit "$status"
