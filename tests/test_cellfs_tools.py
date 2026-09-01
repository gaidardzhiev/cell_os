#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "scripts" / "pack_disk.py"
MKFS = ROOT / "scripts" / "mkcellfs.py"
SYNC = ROOT / "scripts" / "sync_cellfs.py"

P_STAGE2_SECTORS = 0xA55A
P_STAGE2_LBA = 0x1122334455667788
P_KERNEL_SECTORS = 0x5AA5
P_KERNEL_LBA = 0x8877665544332211
P_KERNEL_BYTES = 0xCAFEBABEDEADBEEF
P_MODEL_LBA = 0x13579BDF2468ACE0
P_MODEL_BYTES = 0x0F1E2D3C4B5A6978
P_CELLFS_SECTORS = 0xA1B2C3D4


def put(buf, offset, value, size):
    buf[offset:offset + size] = value.to_bytes(size, "little")


def main():
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        fs = d / "cellfs.img"
        subprocess.run([sys.executable, str(MKFS), str(fs), "--sectors", "64"], check=True,
                       stdout=subprocess.DEVNULL)
        fs_bytes = fs.read_bytes()

        # Persistence synchronizer copies the writable disk tail back to state.
        changed = bytearray(fs_bytes)
        changed[-1] = 0xA5
        disk_sync = d / "sync-disk.img"
        disk_sync.write_bytes(b"prefix" * 123 + changed)
        subprocess.run([sys.executable, str(SYNC), str(disk_sync), str(fs)], check=True,
                       stdout=subprocess.DEVNULL)
        assert fs.read_bytes() == changed
        fs_bytes = fs.read_bytes()

        stage1 = bytearray(512)
        put(stage1, 100, P_STAGE2_SECTORS, 2)
        put(stage1, 120, P_STAGE2_LBA, 8)
        stage2 = bytearray(1024)
        put(stage2, 80, P_KERNEL_SECTORS, 2)
        put(stage2, 96, P_KERNEL_LBA, 8)
        put(stage2, 112, P_KERNEL_BYTES, 8)
        put(stage2, 128, P_MODEL_LBA, 8)
        put(stage2, 144, P_MODEL_BYTES, 8)
        put(stage2, 160, P_CELLFS_SECTORS, 4)
        kernel = bytes([0xCC]) * 700
        model = bytes([0x5A]) * 1000

        p1, p2, pk, pm, out = [d / n for n in ("s1", "s2", "kernel", "model", "disk.img")]
        p1.write_bytes(stage1)
        p2.write_bytes(stage2)
        pk.write_bytes(kernel)
        pm.write_bytes(model)
        subprocess.run([
            sys.executable, str(PACK), str(p1), str(p2), str(pk), str(out),
            "--model", str(pm), "--cellfs", str(fs)
        ], check=True, stdout=subprocess.DEVNULL)
        packed = out.read_bytes()

        assert int.from_bytes(packed[100:102], "little") == 2
        assert int.from_bytes(packed[120:128], "little") == 1
        stage2_off = 512
        assert int.from_bytes(packed[stage2_off + 80:stage2_off + 82], "little") == 2
        assert int.from_bytes(packed[stage2_off + 96:stage2_off + 104], "little") == 3
        assert int.from_bytes(packed[stage2_off + 112:stage2_off + 120], "little") == 700
        assert int.from_bytes(packed[stage2_off + 128:stage2_off + 136], "little") == 5
        assert int.from_bytes(packed[stage2_off + 144:stage2_off + 152], "little") == 1000
        assert int.from_bytes(packed[stage2_off + 160:stage2_off + 164], "little") == 64
        assert packed[-len(fs_bytes):] == fs_bytes
        expected_sectors = 1 + 2 + 2 + 2 + 64
        assert len(packed) == expected_sectors * 512

    print("#CELLFS pack layout PASS")
    print("#CELLFS state synchronization PASS")


if __name__ == "__main__":
    main()
