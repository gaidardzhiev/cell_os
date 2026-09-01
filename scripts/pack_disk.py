#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
import math
import sys

PLACE_STAGE2_SECTORS = 0xA55A
PLACE_STAGE2_LBA = 0x1122334455667788
PLACE_KERNEL_SECTORS = 0x5AA5
PLACE_KERNEL_LBA = 0x8877665544332211
PLACE_KERNEL_BYTES = 0xCAFEBABEDEADBEEF
PLACE_MODEL_LBA = 0x13579BDF2468ACE0
PLACE_MODEL_BYTES = 0x0F1E2D3C4B5A6978
PLACE_CELLFS_SECTORS = 0xA1B2C3D4


def read_bytes(path):
  with open(path, "rb") as fh:
    return bytearray(fh.read())


def write_bytes(path, data):
  with open(path, "wb") as fh:
    fh.write(data)


def patch_value(blob, placeholder, value, size):
  needle = placeholder.to_bytes(size, "little")
  idx = blob.find(needle)
  if idx == -1:
    raise SystemExit(f"placeholder {placeholder:#x} not found")
  blob[idx:idx+size] = value.to_bytes(size, "little")


def patch_value_optional(blob, placeholder, value, size):
  needle = placeholder.to_bytes(size, "little")
  idx = blob.find(needle)
  if idx == -1:
    return False
  blob[idx:idx+size] = value.to_bytes(size, "little")
  return True


def pad_to_sectors(buf):
  sectors = math.ceil(len(buf) / 512) or 1
  need = sectors * 512 - len(buf)
  if need:
    buf.extend(b"\0" * need)
  return sectors


def main():
  args = sys.argv[1:]
  model_path = None
  cellfs_path = None
  if "--model" in args:
    i = args.index("--model")
    if i + 1 >= len(args):
      raise SystemExit("--model requires a CWM path")
    model_path = args[i + 1]
    del args[i:i + 2]
  if "--cellfs" in args:
    i = args.index("--cellfs")
    if i + 1 >= len(args):
      raise SystemExit("--cellfs requires a CellFS image path")
    cellfs_path = args[i + 1]
    del args[i:i + 2]
  if len(args) not in (4, 5):
    print("Usage: pack_disk.py stage1.bin stage2.bin kernel.bin [cgf.bin] disk.img [--model cortex.cwm] [--cellfs cellfs.img]")
    sys.exit(1)

  if len(args) == 5:
    stage1_path, stage2_path, kernel_path, cgf_path, disk_path = args
  else:
    stage1_path, stage2_path, kernel_path, disk_path = args
    cgf_path = None

  stage1 = read_bytes(stage1_path)
  stage2 = read_bytes(stage2_path)
  kernel = read_bytes(kernel_path)
  cgf = read_bytes(cgf_path) if cgf_path else None
  model = read_bytes(model_path) if model_path else None
  cellfs = read_bytes(cellfs_path) if cellfs_path else None
  if cellfs and (len(cellfs) < 512 or len(cellfs) % 512):
    raise SystemExit("CellFS image must be a non-empty multiple of 512 bytes")
  if len(stage1) != 512:
    raise SystemExit("stage1.bin must be exactly 512 bytes")

  stage2_lba = 1
  stage2_sectors = math.ceil(len(stage2) / 512)
  kernel_lba = stage2_lba + stage2_sectors
  kernel_sectors = math.ceil(len(kernel) / 512)
  cgf_lba = kernel_lba + kernel_sectors
  cgf_sectors = math.ceil(len(cgf) / 512) if cgf else 0
  model_lba = cgf_lba + cgf_sectors
  model_sectors = math.ceil(len(model) / 512) if model else 0

  patch_value(stage1, PLACE_STAGE2_SECTORS, stage2_sectors, 2)
  patch_value(stage1, PLACE_STAGE2_LBA, stage2_lba, 8)
  patch_value(stage2, PLACE_KERNEL_SECTORS, kernel_sectors, 2)
  patch_value(stage2, PLACE_KERNEL_LBA, kernel_lba, 8)
  patch_value(stage2, PLACE_KERNEL_BYTES, len(kernel), 8)
  if model:
    patch_value(stage2, PLACE_MODEL_LBA, model_lba, 8)
    patch_value(stage2, PLACE_MODEL_BYTES, len(model), 8)
  else:
    patch_value_optional(stage2, PLACE_MODEL_LBA, 0, 8)
    patch_value_optional(stage2, PLACE_MODEL_BYTES, 0, 8)
  patch_value(stage2, PLACE_CELLFS_SECTORS, len(cellfs) // 512 if cellfs else 0, 4)
  if cgf:
    patch_value(stage2, 0xACE1, cgf_sectors, 2)
    patch_value(stage2, 0x445566778899AABB, cgf_lba, 8)
    patch_value(stage2, 0x0BADC0DED15EA5ED, len(cgf), 8)

  stage2_pad = stage2.copy()
  kernel_pad = kernel.copy()
  pad_to_sectors(stage2_pad)
  pad_to_sectors(kernel_pad)
  disk = bytearray()
  disk.extend(stage1)
  disk.extend(stage2_pad)
  disk.extend(kernel_pad)
  if cgf:
    cgf_pad = cgf.copy()
    pad_to_sectors(cgf_pad)
    disk.extend(cgf_pad)
  if model:
    model_pad = model.copy()
    pad_to_sectors(model_pad)
    disk.extend(model_pad)
  cellfs_lba = len(disk) // 512
  if cellfs:
    disk.extend(cellfs)

  write_bytes(disk_path, disk)
  msg = f"# pack-disk: stage2={stage2_sectors} sectors, kernel={kernel_sectors} sectors"
  if cgf:
    msg += f", cgf={cgf_sectors} sectors"
  if model:
    msg += f", cortex={model_sectors} sectors@lba{model_lba}"
  if cellfs:
    msg += f", cellfs={len(cellfs)//512} sectors@lba{cellfs_lba}"
  msg += f", size={len(disk)} bytes"
  print(msg)


if __name__ == "__main__":
  main()
