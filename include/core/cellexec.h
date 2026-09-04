/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define CELL_EXEC_MAGIC 0x31584543u /* CEX1 */
#define CELL_EXEC_VERSION 1u
#define CELL_EXEC_HEADER_BYTES 64u
#define CELL_EXEC_INSN_BYTES 8u
#define CELL_EXEC_REGS 16u
#define CELL_EXEC_MEMORY_MAX (16u * 1024u)
#define CELL_EXEC_GAS_MAX 1000000u
#define CELL_EXEC_FILE_MAX (16u * 1024u)
#define CELL_EXEC_DATA_BASE 0x40000000u
#define CELL_EXEC_F_NONE 0u
#define CELL_EXEC_F_STATIC_MEMORY 1u

typedef enum {
	CELL_EXEC_OP_HALT = 0,
	CELL_EXEC_OP_MOVI = 1,
	CELL_EXEC_OP_MOV = 2,
	CELL_EXEC_OP_ADD = 3,
	CELL_EXEC_OP_ADDI = 4,
	CELL_EXEC_OP_SUB = 5,
	CELL_EXEC_OP_MUL = 6,
	CELL_EXEC_OP_JZ = 7,
	CELL_EXEC_OP_JNZ = 8,
	CELL_EXEC_OP_JMP = 9,
	CELL_EXEC_OP_PUTS = 10,
	CELL_EXEC_OP_CAP = 11,
	CELL_EXEC_OP_LOAD8 = 12,
	CELL_EXEC_OP_STORE8 = 13,
	CELL_EXEC_OP_DIV = 14,
	CELL_EXEC_OP_MOD = 15,
	CELL_EXEC_OP_CMPEQ = 16,
	CELL_EXEC_OP_CMPNE = 17,
	CELL_EXEC_OP_CMPLT = 18,
	CELL_EXEC_OP_CMPLE = 19,
	CELL_EXEC_OP_CMPGT = 20,
	CELL_EXEC_OP_CMPGE = 21,
	CELL_EXEC_OP_EXIT = 22,
	CELL_EXEC_OP_LOAD64 = 23,
	CELL_EXEC_OP_PUTSM = 24,
	CELL_EXEC_OP_PUTC = 25,
	CELL_EXEC_OP_STRLEN = 26,
	CELL_EXEC_OP_ATOI = 27,
	CELL_EXEC_OP_STRCMPC = 28,
	CELL_EXEC_OP_SYSCALL = 29,
	CELL_EXEC_OP_CALL = 30,
	CELL_EXEC_OP_RET = 31,
	CELL_EXEC_OP_OR = 32,
	CELL_EXEC_OP_STORE64 = 33,
	CELL_EXEC_OP_LOAD32 = 34,
	CELL_EXEC_OP_STORE32 = 35,
	CELL_EXEC_OP_FRAME = 36,
	CELL_EXEC_OP_ADDRL = 37,
	CELL_EXEC_OP_CALLN = 38,
	CELL_EXEC_OP_AND = 39,
	CELL_EXEC_OP_XOR = 40,
	CELL_EXEC_OP_SHL = 41,
	CELL_EXEC_OP_SHR = 42,
	CELL_EXEC_OP_STRCMP = 43
} cell_exec_opcode_t;
typedef enum {
	CELL_EXEC_SYS_OPEN = 1,
	CELL_EXEC_SYS_CLOSE = 2,
	CELL_EXEC_SYS_READ = 3,
	CELL_EXEC_SYS_WRITE = 4,
	CELL_EXEC_SYS_LSEEK = 5,
	CELL_EXEC_SYS_ERRNO = 6,
	CELL_EXEC_SYS_MALLOC = 7,
	CELL_EXEC_SYS_FREE = 8,
	CELL_EXEC_SYS_COMPILE = 9,
	CELL_EXEC_SYS_INSTALL_EXEC = 10
} cell_exec_syscall_t;
#define CELL_EXEC_SYSCALL_PACK(nr, arg2) ((int32_t)(((uint32_t)(nr) & 0xffu) | (((uint32_t)(arg2) & 0xffu) << 8)))
#define CELL_EXEC_SYSCALL_NR(imm) ((uint8_t)((uint32_t)(imm) & 0xffu))
#define CELL_EXEC_SYSCALL_ARG2(imm) ((uint8_t)(((uint32_t)(imm) >> 8) & 0xffu))
#define CELL_EXEC_CALL_PACK(target, argc) ((int32_t)(((uint32_t)(target) & 0x00ffffffu) | (((uint32_t)(argc) & 0xffu) << 24)))
#define CELL_EXEC_CALL_TARGET(imm) ((uint32_t)(imm) & 0x00ffffffu)
#define CELL_EXEC_CALL_ARGC(imm) ((uint8_t)(((uint32_t)(imm) >> 24) & 0xffu))
typedef enum {
	CELL_EXEC_OK = 0,
	CELL_EXEC_BAD_ARGUMENT,
	CELL_EXEC_BAD_MAGIC,
	CELL_EXEC_BAD_VERSION,
	CELL_EXEC_BAD_HEADER,
	CELL_EXEC_BAD_SIZE,
	CELL_EXEC_BAD_CRC,
	CELL_EXEC_BAD_ENTRY,
	CELL_EXEC_BAD_OPCODE,
	CELL_EXEC_BAD_REGISTER,
	CELL_EXEC_BAD_BRANCH,
	CELL_EXEC_BAD_DATA,
	CELL_EXEC_BAD_CAPABILITY,
	CELL_EXEC_BAD_MEMORY,
	CELL_EXEC_BAD_GAS
} cell_exec_status_t;
typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t header_bytes;
	uint32_t instruction_bytes;
	uint32_t code_bytes;
	uint32_t data_bytes;
	uint32_t entry_pc;
	uint64_t capability_mask;
	uint32_t memory_bytes;
	uint32_t gas_limit;
	uint32_t flags;
	uint32_t payload_crc32;
	uint32_t total_bytes;
	uint32_t reserved0;
	uint64_t reserved1;
} cell_exec_header_t;
typedef struct __attribute__((packed)) {
	uint8_t opcode;
	uint8_t dst;
	uint8_t a;
	uint8_t b;
	int32_t imm;
} cell_exec_insn_t;
typedef struct {
	const cell_exec_header_t *h;
	const cell_exec_insn_t *code;
	const uint8_t *data;
	uint32_t instruction_count;
	const uint8_t *blob;
	size_t blob_bytes;
} cell_exec_t;
_Static_assert(sizeof(cell_exec_header_t) == CELL_EXEC_HEADER_BYTES, "CellExec-1 header ABI");
_Static_assert(sizeof(cell_exec_insn_t) == CELL_EXEC_INSN_BYTES, "CellExec-1 instruction ABI");
uint32_t cell_exec_crc32(const void *data, size_t bytes);
const char *cell_exec_status_name(cell_exec_status_t status);
cell_exec_status_t cell_exec_open(cell_exec_t *exec, const void *blob, size_t bytes,
	uint64_t known_capability_mask);
