#!/usr/bin/env python3
import argparse
import json
import pathlib
import struct
import sys

HDR_SIZE = 20
QOS_SIZE = 36

def parse_args():
	ap = argparse.ArgumentParser(description="Dump phenotype .pbin")
	ap.add_argument("file", help="path to .pbin")
	ap.add_argument("--json", action="store_true", help="emit JSON for CI")
	return ap.parse_args()

def read_hdr(blob):
	magic, maj, minr, arch, res, n, quantum = struct.unpack('<IHHHHII', blob[:HDR_SIZE])
	return {
		'magic': magic,
		'ver': (maj, minr),
		'arch_id': arch,
		'num_channels': n,
		'quantum_ns': quantum,
	}

ARCH = {1: 'x86_64', 2: 'arm64'}
QOS = {0: 'BE', 1: 'RT'}

def dump(path, emit_json=False):
	data = pathlib.Path(path).read_bytes()
	if len(data) < HDR_SIZE:
		raise SystemExit('file too small')
	hdr = read_hdr(data)
	n = hdr['num_channels']
	qos_off = HDR_SIZE
	qos_end = qos_off + n * QOS_SIZE
	if qos_end > len(data):
		raise SystemExit('truncated QoS array')
	bitset = data[qos_end:]
	magic_ascii = struct.pack('<I', hdr['magic']).decode('ascii', errors='replace')
	out = {
		"magic": magic_ascii,
		"version": {"major": hdr['ver'][0], "minor": hdr['ver'][1]},
		"hdr_size": HDR_SIZE,
		"arch_id": hdr['arch_id'],
		"arch": ARCH.get(hdr['arch_id'], 'unknown'),
		"channels": n,
		"quantum_ns": hdr['quantum_ns'],
		"size": len(data),
	}
	if emit_json:
		print(json.dumps(out, sort_keys=True))
		return
	print(f"file: {path}")
	print(f"magic: 0x{hdr['magic']:08X} ({out['magic']})")
	print(f"version: {hdr['ver'][0]}.{hdr['ver'][1]}")
	print(f"arch_id: {hdr['arch_id']} ({out['arch']})")
	print(f"channels: {n}")
	print(f"quantum_ns: {hdr['quantum_ns']}")
	print(f"adjacency bytes: {len(bitset)}")
	for idx in range(min(n, 4)):
		base = qos_off + idx * QOS_SIZE
		cid, qos, pri, bucket, bmax, refill = struct.unpack('<IIIQQQ', data[base:base+QOS_SIZE])
		print(f"  channel[{idx}]: id={cid} qos={QOS.get(qos, qos)} pr={pri} bucket={bucket} bucket_max={bmax} refill={refill}")
	if n > 4:
		print("  ...")

if __name__ == '__main__':
	args = parse_args()
	dump(args.file, emit_json=args.json)
