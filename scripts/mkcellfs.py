#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
import argparse
import struct
import zlib
from pathlib import Path

MAGIC = 0x31534643
VERSION = 1
SECTOR = 512
MAX_INODES = 64
INODE_BYTES = 128
INODE_SECTORS = (MAX_INODES * INODE_BYTES) // SECTOR
DATA_LBA = 1 + INODE_SECTORS
TYPE_DIR = 1


def inode_record(index, parent, kind, name, generation):
    name_b = name.encode("ascii")
    if len(name_b) > 63:
        raise ValueError("inode name too long")
    head = struct.pack(
        "<IIBBHIIII64s32s",
        index,
        parent,
        kind,
        0,
        0,
        0,
        0,
        0,
        generation,
        name_b.ljust(64, b"\0"),
        b"\0" * 32,
    )
    assert len(head) == 124
    crc = zlib.crc32(head) & 0xFFFFFFFF
    return head + struct.pack("<I", crc)


def superblock(total_sectors, generation=1):
    head = struct.pack(
        "<IHHIIIIIIIIII",
        MAGIC,
        VERSION,
        SECTOR,
        SECTOR,
        total_sectors,
        MAX_INODES,
        INODE_BYTES,
        1,
        INODE_SECTORS,
        DATA_LBA,
        DATA_LBA,
        generation,
        0,
    )
    assert len(head) == 48
    raw = bytearray(head + struct.pack("<I", 0) + b"\0" * 460)
    assert len(raw) == 512
    crc = zlib.crc32(raw) & 0xFFFFFFFF
    raw[48:52] = struct.pack("<I", crc)
    return bytes(raw)


def make_image(sectors):
    if sectors <= DATA_LBA:
        raise ValueError(f"CellFS requires more than {DATA_LBA} sectors")
    image = bytearray(sectors * SECTOR)
    image[0:SECTOR] = superblock(sectors)
    entries = [
        inode_record(1, 1, TYPE_DIR, "", 1),
        inode_record(2, 1, TYPE_DIR, "home", 1),
        inode_record(3, 1, TYPE_DIR, "programs", 1),
    ]
    zero_head = b"\0" * 124
    zero_inode = zero_head + struct.pack("<I", zlib.crc32(zero_head) & 0xFFFFFFFF)
    entries.extend([zero_inode] * (MAX_INODES - len(entries)))
    table = b"".join(entries)
    assert len(table) == MAX_INODES * INODE_BYTES
    image[SECTOR:SECTOR + len(table)] = table
    return bytes(image)


def main():
    ap = argparse.ArgumentParser(description="Create a CellFS-1 persistent volume")
    ap.add_argument("output")
    ap.add_argument("--sectors", type=int, default=1024)
    args = ap.parse_args()
    data = make_image(args.sectors)
    p = Path(args.output)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    print(f"#CELLFS formatted: {p} sectors={args.sectors} bytes={len(data)} data_lba={DATA_LBA}")


if __name__ == "__main__":
    main()
