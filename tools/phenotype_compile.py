#!/usr/bin/env python3
import sys
import struct
import hashlib
import os
import yaml

ARCH = {"x86_64": 1, "arm64": 2}
QOS  = {"BE": 0, "RT": 1}

le16 = lambda x: struct.pack("<H", int(x))
le32 = lambda x: struct.pack("<I", int(x))
le64 = lambda x: struct.pack("<Q", int(x))

def emit_pbin(yaml_path, out_path):
	with open(yaml_path, "r", encoding="utf-8") as f:
		data = yaml.safe_load(f)

	arch = ARCH[data["arch"]]
	chans = data["channels"]
	n = len(chans)
	quantum = int(data["schedules"]["quantum_ns"])

	blob = bytearray()
	blob += le32(0x50484E4F)
	blob += le16(1) + le16(0)
	blob += le16(arch) + le16(0)
	blob += le32(n)
	blob += le32(quantum)

	for ch in chans:
		cid    = int(ch["id"])
		qos    = QOS[ch["qos"]]
		pr     = int(ch["priority"])
		refill = int(ch.get("refill_per_tick", 0))
		bmax   = int(ch.get("bucket_max", 0))
		blob += le32(cid)
		blob += le32(qos)
		blob += le32(pr)
		blob += le64(bmax)
		blob += le64(bmax)
		blob += le64(refill)

	bits = n * n
	bs   = bytearray((bits + 7) // 8)
	for i in range(n):
		for j in range(n):
			idx = i * n + j
			bs[idx >> 3] |= (1 << (idx & 7))
	blob += bs

	with open(out_path, "wb") as out:
		out.write(blob)

	digest = hashlib.sha256(blob).hexdigest()
	sha_path = out_path + ".sha256"
	with open(sha_path, "w", encoding="utf-8") as sf:
		sf.write(f"{digest}  {os.path.basename(out_path)}\n")

def main():
	if len(sys.argv) != 3:
		print("usage: phenotype_compile.py <in.yaml> <out.pbin>")
		sys.exit(2)
	emit_pbin(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
	main()
