#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
import argparse
import struct
import zlib
from pathlib import Path

SECTOR = 512
MAGIC = 0x31534643
VERSION = 1
MAX_INODES = 64
INODE_BYTES = 128
TYPE_FILE = 2
SUPER_HEAD = struct.Struct("<IHH10I")
INODE = struct.Struct("<IIBBHIIII64s32sI")

def crc32(b): return zlib.crc32(b) & 0xFFFFFFFF

def verify_super(image):
    raw = bytearray(image[:SECTOR])
    vals = SUPER_HEAD.unpack_from(raw, 0)
    magic, version, header = vals[:3]
    if magic != MAGIC or version != VERSION or header != SECTOR:
        raise ValueError("invalid CellFS superblock")
    stored = struct.unpack_from("<I", raw, 48)[0]
    raw[48:52] = b"\0" * 4
    if crc32(raw) != stored:
        raise ValueError("CellFS superblock CRC mismatch")
    return list(vals), stored

def inode_offset(index): return SECTOR + index * INODE_BYTES

def parse_inode(image, index):
    raw = bytes(image[inode_offset(index):inode_offset(index)+INODE_BYTES])
    fields = list(INODE.unpack(raw))
    head = raw[:124]
    if crc32(head) != fields[-1]:
        raise ValueError(f"inode {index+1} CRC mismatch")
    return fields

def pack_inode(fields):
    tmp = INODE.pack(*fields[:-1], 0)
    return tmp[:124] + struct.pack("<I", crc32(tmp[:124]))

def inode_name(fields): return fields[9].split(b"\0",1)[0].decode("ascii")

def install(image_path, program_path, name):
    p = Path(image_path)
    image = bytearray(p.read_bytes())
    super_fields, _ = verify_super(image)
    # IHH10I: total_sectors is item 4, next_data_lba item 10, generation item 11.
    total_sectors = super_fields[4]
    next_lba = super_fields[10]
    generation = super_fields[11]
    if len(image) != total_sectors * SECTOR:
        raise ValueError("CellFS image size mismatch")
    if not name or "/" in name or len(name.encode("ascii")) > 63:
        raise ValueError("invalid program file name")
    inodes = [parse_inode(image, i) for i in range(MAX_INODES)]
    programs_id = None
    for f in inodes:
        if f[2] == 1 and f[1] == 1 and inode_name(f) == "programs":
            programs_id = f[0]
            break
    if programs_id is None:
        raise ValueError("/programs directory missing")
    blob = Path(program_path).read_bytes()
    existing_index = None
    legacy_index = None
    free_index = None
    legacy_name = Path(program_path).name
    for i, f in enumerate(inodes):
        if f[2] == 0 and free_index is None and i != 0:
            free_index = i
        if f[2] != 0 and f[1] == programs_id:
            current_name = inode_name(f)
            if current_name == name:
                existing_index = i
                break
            if name != legacy_name and current_name == legacy_name:
                legacy_index = i
    if existing_index is None and legacy_index is not None:
        existing_index = legacy_index
    if existing_index is not None:
        f = inodes[existing_index]
        old_name = inode_name(f)
        old = bytes(image[f[6]*SECTOR:f[6]*SECTOR+f[5]]) if f[5] else b""
        if old == blob and old_name == name:
            print(f"#CELLFS program unchanged: /programs/{name} bytes={len(blob)} generation={generation}")
            return False
        if old == blob and old_name != name:
            generation += 1
            f[8] = generation
            f[9] = name.encode("ascii").ljust(64, b"\0")
            image[inode_offset(existing_index):inode_offset(existing_index)+INODE_BYTES] = pack_inode(f)
            super_fields[11] = generation
            head = SUPER_HEAD.pack(*super_fields)
            raw = bytearray(head + struct.pack("<I", 0) + bytes(image[52:SECTOR]))
            raw[48:52] = struct.pack("<I", crc32(raw))
            image[:SECTOR] = raw
            p.write_bytes(image)
            print(f"#CELLFS program renamed: /programs/{old_name} -> /programs/{name} generation={generation}")
            return True
        index = existing_index
        inode_id = f[0]
    else:
        if free_index is None:
            raise ValueError("CellFS inode table full")
        index = free_index
        inode_id = index + 1
    sectors = (len(blob) + SECTOR - 1) // SECTOR
    if next_lba + sectors > total_sectors:
        raise ValueError("CellFS data area full")
    if sectors:
        padded = blob + b"\0" * (sectors * SECTOR - len(blob))
        image[next_lba*SECTOR:(next_lba+sectors)*SECTOR] = padded
    generation += 1
    name_b = name.encode("ascii").ljust(64, b"\0")
    fields = [inode_id, programs_id, TYPE_FILE, 0, 0, len(blob), next_lba if sectors else 0,
              sectors, generation, name_b, b"\0"*32, 0]
    image[inode_offset(index):inode_offset(index)+INODE_BYTES] = pack_inode(fields)
    super_fields[10] = next_lba + sectors
    super_fields[11] = generation
    head = SUPER_HEAD.pack(*super_fields)
    raw = bytearray(head + struct.pack("<I", 0) + bytes(image[52:SECTOR]))
    raw[48:52] = struct.pack("<I", crc32(raw))
    image[:SECTOR] = raw
    p.write_bytes(image)
    print(f"#CELLFS program installed: /programs/{name} bytes={len(blob)} generation={generation}")
    return True

def main():
    ap = argparse.ArgumentParser(description="Install a CellExec-1 image into /programs without reformatting CellFS")
    ap.add_argument("cellfs")
    ap.add_argument("program")
    ap.add_argument("--name")
    args = ap.parse_args()
    name = args.name or Path(args.program).name
    install(args.cellfs, args.program, name)

if __name__ == "__main__": main()
