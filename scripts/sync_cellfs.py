#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
import struct
import sys
from pathlib import Path

MAGIC = 0x31534643
SECTOR = 512


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: sync_cellfs.py disk.img state/cellfs.img")
    disk_path = Path(sys.argv[1])
    state_path = Path(sys.argv[2])
    disk = disk_path.read_bytes()
    state = state_path.read_bytes()
    if len(state) < SECTOR or len(state) % SECTOR:
        raise SystemExit("invalid CellFS state image size")
    magic, = struct.unpack_from("<I", state, 0)
    if magic != MAGIC:
        raise SystemExit("state image is not CellFS-1")
    sectors, = struct.unpack_from("<I", state, 12)
    expected = sectors * SECTOR
    if expected != len(state):
        raise SystemExit("CellFS state size does not match superblock")
    if len(disk) < expected:
        raise SystemExit("disk image is smaller than CellFS volume")
    tail = disk[-expected:]
    tail_magic, = struct.unpack_from("<I", tail, 0)
    if tail_magic != MAGIC:
        raise SystemExit("disk tail does not contain CellFS-1")
    tmp = state_path.with_suffix(state_path.suffix + ".tmp")
    tmp.write_bytes(tail)
    tmp.replace(state_path)
    print(f"#CELLFS state synchronized: {state_path} bytes={expected}")


if __name__ == "__main__":
    main()
