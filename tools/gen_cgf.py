#!/usr/bin/env python3
import argparse
import struct

HEADER_SIZE = 4096


def crc32c(data: bytes) -> int:
  crc = 0xFFFFFFFF
  for b in data:
    crc ^= b
    for _ in range(8):
      mask = -(crc & 1) & 0xFFFFFFFF
      crc = ((crc >> 1) ^ (0x82F63B78 & mask)) & 0xFFFFFFFF
  return crc ^ 0xFFFFFFFF


def make_header(genedir_len: int, genedir_off: int, corrupt_crc: bool) -> bytes:
  hdr = bytearray(HEADER_SIZE)
  struct.pack_into("<II", hdr, 0, 0, genedir_off)
  struct.pack_into("<I", hdr, 8, genedir_len)
  crc = crc32c(bytes(hdr))
  if corrupt_crc:
    crc ^= 0xFFFFFFFF  # deterministic wrong value
  struct.pack_into("<I", hdr, 0, crc)
  return bytes(hdr)


def main():
  ap = argparse.ArgumentParser(description="Generate deterministic CGF v1 blob")
  ap.add_argument("out", help="Output CGF file")
  ap.add_argument("--corrupt-crc", action="store_true", help="Write a bad CRC32C for negative testing")
  args = ap.parse_args()

  genedir = bytearray()
  for i in range(256):
    genedir.append(i & 0xFF)
  for i in range(256):
    genedir.append((255 - i) & 0xFF)
  genedir_bytes = bytes(genedir)
  genedir_off = HEADER_SIZE
  hdr = make_header(len(genedir_bytes), genedir_off, args.corrupt_crc)

  with open(args.out, "wb") as fh:
    fh.write(hdr)
    fh.write(genedir_bytes)

  crc_disp = crc32c(hdr)
  status = "BAD" if args.corrupt_crc else "OK"
  print(f"# gen_cgf: len={HEADER_SIZE + len(genedir_bytes)} bytes crc32c={crc_disp:08X} ({status})")


if __name__ == "__main__":
  main()
