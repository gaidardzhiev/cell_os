#!/usr/bin/env python3
import math
import struct
import sys

PLACE_STAGE2_SECTORS = 0xA55A
PLACE_STAGE2_LBA = 0x1122334455667788
PLACE_KERNEL_SECTORS = 0x5AA5
PLACE_KERNEL_LBA = 0x8877665544332211
PLACE_KERNEL_BYTES = 0xCAFEBABEDEADBEEF


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


def pad_to_sectors(buf):
  sectors = math.ceil(len(buf) / 512) or 1
  need = sectors * 512 - len(buf)
  if need:
    buf.extend(b"\0" * need)
  return sectors


def main():
  if len(sys.argv) not in (5, 6):
    print("Usage: pack_disk.py stage1.bin stage2.bin kernel.bin [cgf.bin] disk.img")
    sys.exit(1)

  if len(sys.argv) == 6:
    stage1_path, stage2_path, kernel_path, cgf_path, disk_path = sys.argv[1:]
  else:
    stage1_path, stage2_path, kernel_path, disk_path = sys.argv[1:]
    cgf_path = None

  stage1 = read_bytes(stage1_path)
  stage2 = read_bytes(stage2_path)
  kernel = read_bytes(kernel_path)
  cgf = read_bytes(cgf_path) if cgf_path else None

  if len(stage1) != 512:
    raise SystemExit("stage1.bin must be exactly 512 bytes")

  stage2_lba = 1
  stage2_sectors = math.ceil(len(stage2) / 512)
  kernel_lba = stage2_lba + stage2_sectors
  kernel_sectors = math.ceil(len(kernel) / 512)
  cgf_lba = kernel_lba + kernel_sectors
  cgf_sectors = math.ceil(len(cgf) / 512) if cgf else 0

  patch_value(stage1, PLACE_STAGE2_SECTORS, stage2_sectors, 2)
  patch_value(stage1, PLACE_STAGE2_LBA, stage2_lba, 8)

  patch_value(stage2, PLACE_KERNEL_SECTORS, kernel_sectors, 2)
  patch_value(stage2, PLACE_KERNEL_LBA, kernel_lba, 8)
  patch_value(stage2, PLACE_KERNEL_BYTES, len(kernel), 8)
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

  write_bytes(disk_path, disk)
  msg = f"# pack-disk: stage2={stage2_sectors} sectors, kernel={kernel_sectors} sectors"
  if cgf:
    msg += f", cgf={cgf_sectors} sectors"
  msg += f", size={len(disk)} bytes"
  print(msg)


if __name__ == "__main__":
  main()
