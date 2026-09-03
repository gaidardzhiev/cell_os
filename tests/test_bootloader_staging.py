#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later

from pathlib import Path
import re

stage2 = Path("src/boot/stage2.asm").read_text()
makefile = Path("Makefile").read_text()


def define_int(name):
    m = re.search(rf"(?m)^%define\s+{re.escape(name)}\s+(0x[0-9A-Fa-f]+|[0-9]+)\s*$", stage2)
    if not m:
        raise SystemExit(f"#BOOTLOADER staging FAIL missing {name}")
    return int(m.group(1), 0)


stage = define_int("KERNEL_STAGE_ADDR")
chunk = define_int("KERNEL_BIOS_CHUNK_SECTORS")
limit = define_int("KERNEL_STAGE_LIMIT_BYTES")
stack = define_int("STACK_TOP")

if stage != 0x40000:
    raise SystemExit("#BOOTLOADER staging FAIL staging address")
if not (1 <= chunk <= 127):
    raise SystemExit("#BOOTLOADER staging FAIL BIOS chunk bound")
if chunk * 512 != 0x8000:
    raise SystemExit("#BOOTLOADER staging FAIL expected 32 KiB chunk")
if limit != 262144:
    raise SystemExit("#BOOTLOADER staging FAIL staging limit")
if stage + limit > 0x80000:
    raise SystemExit("#BOOTLOADER staging FAIL staging high-water mark")
if stage + limit >= stack:
    raise SystemExit("#BOOTLOADER staging FAIL staging overlaps stack")

required = [
    ".kernel_read_loop:",
    ".kernel_chunk_ready:",
    ".kernel_read_done:",
    "kernel_sectors_left: dw 0",
    "kernel_chunk_sectors: dw 0",
    "mov [dap3_secs], ax",
    "sub [kernel_sectors_left], ax",
    "add dword [dap3_lba], eax",
    "adc dword [dap3_lba+4], 0",
    "shl ax, 5",
    "add [dap3_buf_seg], ax",
    "dap3_secs: dw 0x5AA5",
    "dap3_lba: dq 0x8877665544332211",
]
for item in required:
    if item not in stage2:
        raise SystemExit(f"#BOOTLOADER staging FAIL missing {item}")

if "test $$bytes -le 262144" not in makefile:
    raise SystemExit("#BOOTLOADER staging FAIL Makefile staging guard")
if "test-bootloader-staging" not in makefile:
    raise SystemExit("#BOOTLOADER staging FAIL regression target")

# Verify the chunk schedule for every legal kernel sector count. Each transfer
# must be <= 127 sectors, stay below the staging ceiling, and not straddle a
# physical 64 KiB boundary.
for total in range(1, limit // 512 + 1):
    left = total
    addr = stage
    while left:
        n = min(left, chunk)
        size = n * 512
        if n > 127:
            raise SystemExit("#BOOTLOADER staging FAIL transfer exceeds BIOS bound")
        if addr + size > stage + limit:
            raise SystemExit("#BOOTLOADER staging FAIL transfer exceeds staging area")
        if (addr >> 16) != ((addr + size - 1) >> 16):
            raise SystemExit("#BOOTLOADER staging FAIL transfer crosses 64 KiB boundary")
        addr += size
        left -= n

print(f"#BOOTLOADER staging chunking PASS stage=0x{stage:x} chunk={chunk} limit={limit}")
