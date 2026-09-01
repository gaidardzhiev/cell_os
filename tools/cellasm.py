#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.
import argparse
import ast
import shlex
import struct
import zlib
from pathlib import Path

MAGIC = 0x31584543
VERSION = 1
HEADER_BYTES = 64
INSN_BYTES = 8
MEM_MAX = 4096
GAS_MAX = 100000
FILE_MAX = 16 * 1024

CAPS = {
    "system.status": 1,
    "cpu.info": 2,
    "memory.status": 3,
    "storage.list": 4,
    "storage.list_dir": 5,
    "storage.pwd": 6,
    "network.list": 7,
    "gpu.info": 8,
    "usb.list": 9,
    "display.info": 10,
    "power.status": 11,
}
OPS = {
    "halt": 0, "movi": 1, "mov": 2, "add": 3, "addi": 4, "sub": 5,
    "mul": 6, "jz": 7, "jnz": 8, "jmp": 9, "puts": 10, "cap": 11,
    "load8": 12, "store8": 13,
}

def reg(s):
    if not s.startswith("r") or not s[1:].isdigit():
        raise ValueError(f"invalid register: {s}")
    v = int(s[1:])
    if not 0 <= v < 16:
        raise ValueError(f"register out of range: {s}")
    return v

def integer(s):
    return int(s, 0)

def clean_lines(text):
    out = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.split(";", 1)[0].strip()
        if line:
            out.append((lineno, line))
    return out

def assemble(text):
    lines = clean_lines(text)
    labels = {}
    data_defs = []
    insn_defs = []
    caps = set()
    gas = 1024
    memory = 256
    entry_label = None
    pc = 0

    for lineno, line in lines:
        if line.endswith(":"):
            name = line[:-1].strip()
            if not name or name in labels:
                raise ValueError(f"line {lineno}: invalid/duplicate label")
            labels[name] = pc
            continue
        if line.startswith("."):
            parts = shlex.split(line, posix=True)
            d = parts[0]
            if d == ".gas" and len(parts) == 2:
                gas = integer(parts[1])
            elif d == ".memory" and len(parts) == 2:
                memory = integer(parts[1])
            elif d == ".cap" and len(parts) == 2:
                if parts[1] not in CAPS:
                    raise ValueError(f"line {lineno}: unknown capability {parts[1]}")
                caps.add(parts[1])
            elif d == ".entry" and len(parts) == 2:
                entry_label = parts[1]
            elif d == ".data":
                # Preserve quoted payload by splitting directive and label first.
                rest = line[len(".data"):].strip()
                label, sep, literal = rest.partition(" ")
                if not sep or not label:
                    raise ValueError(f"line {lineno}: .data NAME \"text\"")
                value = ast.literal_eval(literal.strip())
                if not isinstance(value, str):
                    raise ValueError(f"line {lineno}: data must be a string")
                try:
                    b = value.encode("ascii")
                except UnicodeEncodeError as e:
                    raise ValueError(f"line {lineno}: CellExec-1 data is ASCII-only") from e
                if not b or len(b) > 65535:
                    raise ValueError(f"line {lineno}: invalid data length")
                data_defs.append((label, b))
            else:
                raise ValueError(f"line {lineno}: invalid directive")
            continue
        parts = shlex.split(line, posix=True)
        op = parts[0].lower()
        if op not in OPS:
            raise ValueError(f"line {lineno}: unknown opcode {op}")
        insn_defs.append((lineno, parts))
        pc += 1

    if not insn_defs:
        raise ValueError("program has no instructions")
    if not 1 <= gas <= GAS_MAX:
        raise ValueError("gas outside CellExec-1 bounds")
    if not 0 <= memory <= MEM_MAX:
        raise ValueError("memory outside CellExec-1 bounds")
    if entry_label is None:
        entry_pc = 0
    else:
        if entry_label not in labels:
            raise ValueError(f"unknown entry label: {entry_label}")
        entry_pc = labels[entry_label]

    data = bytearray()
    data_map = {}
    for label, b in data_defs:
        if label in data_map:
            raise ValueError(f"duplicate data label: {label}")
        data_map[label] = (len(data), len(b))
        data += b

    code = bytearray()
    for pc, (lineno, p) in enumerate(insn_defs):
        op = p[0].lower()
        dst = a = b = 0
        imm = 0
        try:
            if op == "halt":
                if len(p) not in (1, 2): raise ValueError("halt [CODE]")
                imm = integer(p[1]) if len(p) == 2 else 0
            elif op == "movi":
                if len(p) != 3: raise ValueError("movi rD IMM")
                dst, imm = reg(p[1]), integer(p[2])
            elif op == "mov":
                if len(p) != 3: raise ValueError("mov rD rA")
                dst, a = reg(p[1]), reg(p[2])
            elif op in ("add", "sub", "mul"):
                if len(p) != 4: raise ValueError(f"{op} rD rA rB")
                dst, a, b = reg(p[1]), reg(p[2]), reg(p[3])
            elif op == "addi":
                if len(p) != 4: raise ValueError("addi rD rA IMM")
                dst, a, imm = reg(p[1]), reg(p[2]), integer(p[3])
            elif op in ("jz", "jnz"):
                if len(p) != 3: raise ValueError(f"{op} rA LABEL")
                a = reg(p[1])
                if p[2] not in labels: raise ValueError(f"unknown label {p[2]}")
                imm = labels[p[2]] - (pc + 1)
            elif op == "jmp":
                if len(p) != 2: raise ValueError("jmp LABEL")
                if p[1] not in labels: raise ValueError(f"unknown label {p[1]}")
                imm = labels[p[1]] - (pc + 1)
            elif op == "puts":
                if len(p) != 2 or p[1] not in data_map: raise ValueError("puts DATA_LABEL")
                imm, n = data_map[p[1]]
                a, b = n & 0xFF, (n >> 8) & 0xFF
            elif op == "cap":
                if len(p) not in (2, 3): raise ValueError("cap NAME [rD]")
                name = p[1]
                if name not in CAPS: raise ValueError(f"unknown capability {name}")
                if name not in caps: raise ValueError(f"capability {name} not declared with .cap")
                imm = CAPS[name]
                dst = reg(p[2]) if len(p) == 3 else 0
            elif op == "load8":
                if len(p) != 4: raise ValueError("load8 rD rA OFFSET")
                dst, a, imm = reg(p[1]), reg(p[2]), integer(p[3])
            elif op == "store8":
                if len(p) != 4: raise ValueError("store8 rA rB OFFSET")
                a, b, imm = reg(p[1]), reg(p[2]), integer(p[3])
        except ValueError as e:
            raise ValueError(f"line {lineno}: {e}") from e
        if not -(1 << 31) <= imm < (1 << 31):
            raise ValueError(f"line {lineno}: immediate outside int32")
        code += struct.pack("<BBBBi", OPS[op], dst, a, b, imm)

    cap_mask = 0
    for name in caps:
        cap_mask |= 1 << CAPS[name]
    payload = bytes(code + data)
    total = HEADER_BYTES + len(payload)
    if total > FILE_MAX:
        raise ValueError("program exceeds CellFS/CellExec-1 file limit")
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = struct.pack(
        "<IHHIIIIQIIIIIIQ",
        MAGIC, VERSION, HEADER_BYTES, INSN_BYTES, len(code), len(data), entry_pc,
        cap_mask, memory, gas, 0, crc, total, 0, 0,
    )
    assert len(header) == HEADER_BYTES
    return header + payload, len(insn_defs), cap_mask, gas, memory, crc

def main():
    ap = argparse.ArgumentParser(description="Assemble CellExec-1 bytecode")
    ap.add_argument("source")
    ap.add_argument("output")
    args = ap.parse_args()
    src = Path(args.source)
    blob, insns, caps, gas, memory, crc = assemble(src.read_text(encoding="utf-8"))
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(blob)
    print(f"#CELLEXEC assembled: {out} bytes={len(blob)} insns={insns} caps=0x{caps:016x} gas={gas} memory={memory} crc={crc:08x}")

if __name__ == "__main__":
    main()
