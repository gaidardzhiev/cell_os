#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="${1:-$ROOT/build/disk.img}"

if [[ ! -f "$IMG" ]]; then
	make -C "$ROOT" bios-smoke
fi

exec qemu-system-x86_64 \
	-machine pc -cpu max -m 64M \
	-nographic -monitor none \
	-serial none -no-reboot -no-shutdown \
	-chardev stdio,id=dbg \
	-device isa-debugcon,iobase=0xE9,chardev=dbg \
	-drive file="${IMG}",format=raw,if=ide
