/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/cellexec.h"

static int reg_ok(uint8_t r) { return r < CELL_EXEC_REGS; }

uint32_t cell_exec_crc32(const void *data, size_t bytes) {
	const uint8_t *p = (const uint8_t *)data;
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < bytes; ++i) {
		crc ^= p[i];
		for (unsigned b = 0; b < 8u; ++b)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

const char *cell_exec_status_name(cell_exec_status_t status) {
	switch (status) {
	case CELL_EXEC_OK: return "ok";
	case CELL_EXEC_BAD_ARGUMENT: return "bad_argument";
	case CELL_EXEC_BAD_MAGIC: return "bad_magic";
	case CELL_EXEC_BAD_VERSION: return "bad_version";
	case CELL_EXEC_BAD_HEADER: return "bad_header";
	case CELL_EXEC_BAD_SIZE: return "bad_size";
	case CELL_EXEC_BAD_CRC: return "bad_crc";
	case CELL_EXEC_BAD_ENTRY: return "bad_entry";
	case CELL_EXEC_BAD_OPCODE: return "bad_opcode";
	case CELL_EXEC_BAD_REGISTER: return "bad_register";
	case CELL_EXEC_BAD_BRANCH: return "bad_branch";
	case CELL_EXEC_BAD_DATA: return "bad_data";
	case CELL_EXEC_BAD_CAPABILITY: return "bad_capability";
	case CELL_EXEC_BAD_MEMORY: return "bad_memory";
	case CELL_EXEC_BAD_GAS: return "bad_gas";
	default: return "unknown";
	}
}

static int branch_target_ok(uint32_t pc, int32_t rel, uint32_t count) {
	int64_t target = (int64_t)pc + 1 + (int64_t)rel;
	return target >= 0 && target < (int64_t)count;
}

static int data_cstr_ok(const uint8_t *data, uint32_t bytes, uint32_t off) {
	if (off >= bytes) return 0;
	for (uint32_t i = off; i < bytes; ++i) if (data[i] == 0u) return 1;
	return 0;
}

cell_exec_status_t cell_exec_open(cell_exec_t *exec, const void *blob, size_t bytes,
	uint64_t known_capability_mask) {
	if (!exec || !blob || bytes < CELL_EXEC_HEADER_BYTES) return CELL_EXEC_BAD_ARGUMENT;
	const cell_exec_header_t *h = (const cell_exec_header_t *)blob;
	if (h->magic != CELL_EXEC_MAGIC) return CELL_EXEC_BAD_MAGIC;
	if (h->version != CELL_EXEC_VERSION) return CELL_EXEC_BAD_VERSION;
	if (h->header_bytes != CELL_EXEC_HEADER_BYTES || h->instruction_bytes != CELL_EXEC_INSN_BYTES ||
	    h->flags != CELL_EXEC_F_NONE || h->reserved0 != 0u || h->reserved1 != 0u)
		return CELL_EXEC_BAD_HEADER;
	if (!h->code_bytes || (h->code_bytes % CELL_EXEC_INSN_BYTES) != 0u)
		return CELL_EXEC_BAD_SIZE;
	uint64_t payload_bytes = (uint64_t)h->code_bytes + (uint64_t)h->data_bytes;
	uint64_t total = (uint64_t)CELL_EXEC_HEADER_BYTES + payload_bytes;
	if (total > CELL_EXEC_FILE_MAX || total != bytes || h->total_bytes != (uint32_t)total)
		return CELL_EXEC_BAD_SIZE;
	if (cell_exec_crc32((const uint8_t *)blob + CELL_EXEC_HEADER_BYTES, (size_t)payload_bytes) != h->payload_crc32)
		return CELL_EXEC_BAD_CRC;
	uint32_t count = h->code_bytes / CELL_EXEC_INSN_BYTES;
	if (h->entry_pc >= count) return CELL_EXEC_BAD_ENTRY;
	if (h->memory_bytes > CELL_EXEC_MEMORY_MAX) return CELL_EXEC_BAD_MEMORY;
	if (!h->gas_limit || h->gas_limit > CELL_EXEC_GAS_MAX) return CELL_EXEC_BAD_GAS;
	if (h->capability_mask & ~known_capability_mask) return CELL_EXEC_BAD_CAPABILITY;

	const cell_exec_insn_t *code = (const cell_exec_insn_t *)((const uint8_t *)blob + CELL_EXEC_HEADER_BYTES);
	const uint8_t *data = (const uint8_t *)blob + CELL_EXEC_HEADER_BYTES + h->code_bytes;
	for (uint32_t pc = 0; pc < count; ++pc) {
		const cell_exec_insn_t *in = &code[pc];
		switch (in->opcode) {
		case CELL_EXEC_OP_HALT:
			break;
		case CELL_EXEC_OP_MOVI:
			if (!reg_ok(in->dst)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_MOV:
			if (!reg_ok(in->dst) || !reg_ok(in->a)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_ADD:
		case CELL_EXEC_OP_SUB:
		case CELL_EXEC_OP_MUL:
		case CELL_EXEC_OP_DIV:
		case CELL_EXEC_OP_MOD:
		case CELL_EXEC_OP_CMPEQ:
		case CELL_EXEC_OP_CMPNE:
		case CELL_EXEC_OP_CMPLT:
		case CELL_EXEC_OP_CMPLE:
		case CELL_EXEC_OP_CMPGT:
		case CELL_EXEC_OP_CMPGE:
			if (!reg_ok(in->dst) || !reg_ok(in->a) || !reg_ok(in->b)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_ADDI:
			if (!reg_ok(in->dst) || !reg_ok(in->a)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_JZ:
		case CELL_EXEC_OP_JNZ:
			if (!reg_ok(in->a)) return CELL_EXEC_BAD_REGISTER;
			if (!branch_target_ok(pc, in->imm, count)) return CELL_EXEC_BAD_BRANCH;
			break;
		case CELL_EXEC_OP_JMP:
			if (!branch_target_ok(pc, in->imm, count)) return CELL_EXEC_BAD_BRANCH;
			break;
		case CELL_EXEC_OP_PUTS: {
			if (in->imm < 0) return CELL_EXEC_BAD_DATA;
			uint32_t len = (uint32_t)in->a | ((uint32_t)in->b << 8);
			uint32_t off = (uint32_t)in->imm;
			if (!len || off > h->data_bytes || len > h->data_bytes - off) return CELL_EXEC_BAD_DATA;
			break;
		}
		case CELL_EXEC_OP_CAP: {
			if (in->imm <= 0 || in->imm >= 64) return CELL_EXEC_BAD_CAPABILITY;
			uint64_t bit = 1ull << (uint32_t)in->imm;
			if (!(known_capability_mask & bit) || !(h->capability_mask & bit)) return CELL_EXEC_BAD_CAPABILITY;
			if (!reg_ok(in->dst)) return CELL_EXEC_BAD_REGISTER;
			break;
		}
		case CELL_EXEC_OP_LOAD8:
		case CELL_EXEC_OP_LOAD64:
			if (!reg_ok(in->dst) || !reg_ok(in->a)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_STORE8:
			if (!reg_ok(in->a) || !reg_ok(in->b)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_EXIT:
		case CELL_EXEC_OP_PUTSM:
		case CELL_EXEC_OP_PUTC:
			if (!reg_ok(in->a)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_STRLEN:
		case CELL_EXEC_OP_ATOI:
			if (!reg_ok(in->dst) || !reg_ok(in->a)) return CELL_EXEC_BAD_REGISTER;
			break;
		case CELL_EXEC_OP_STRCMPC:
			if (!reg_ok(in->dst) || !reg_ok(in->a) || in->imm < 0 ||
			    !data_cstr_ok(data, h->data_bytes, (uint32_t)in->imm)) return CELL_EXEC_BAD_DATA;
			break;
		default:
			return CELL_EXEC_BAD_OPCODE;
		}
	}

	exec->h = h;
	exec->code = code;
	exec->data = data;
	exec->instruction_count = count;
	exec->blob = (const uint8_t *)blob;
	exec->blob_bytes = bytes;
	return CELL_EXEC_OK;
}
