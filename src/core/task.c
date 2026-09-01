/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include "core/task.h"
#include "core/capability.h"
#include "core/vfs.h"

typedef struct {
	char *p;
	char *end;
	int ok;
} outbuf_t;

static void ob_init(outbuf_t *b, char *out, size_t cap) {
	b->p = out;
	b->end = out + (cap ? cap - 1u : 0u);
	b->ok = cap != 0u;
	if (cap) out[0] = 0;
}
static void ob_char(outbuf_t *b, char c) {
	if (!b->ok || b->p >= b->end) { b->ok = 0; return; }
	*b->p++ = c; *b->p = 0;
}
static void ob_str(outbuf_t *b, const char *s) { while (s && *s) ob_char(b, *s++); }
static void ob_u32(outbuf_t *b, uint32_t v) {
	char tmp[12]; unsigned n = 0;
	if (!v) { ob_char(b, '0'); return; }
	while (v && n < sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
	while (n) ob_char(b, tmp[--n]);
}
static void ob_i32(outbuf_t *b, int32_t v) {
	if (v < 0) { ob_char(b, '-'); ob_u32(b, (uint32_t)(-(int64_t)v)); }
	else ob_u32(b, (uint32_t)v);
}
static void copy_text(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n]; ++n; }
	dst[n] = 0;
}
static void zero_bytes(void *dst, size_t n) {
	uint8_t *p = (uint8_t *)dst; while (n--) *p++ = 0;
}

const char *cell_task_state_name(cell_task_state_t state) {
	switch (state) {
	case CELL_TASK_EMPTY: return "empty";
	case CELL_TASK_READY: return "ready";
	case CELL_TASK_RUNNING: return "running";
	case CELL_TASK_EXITED: return "exited";
	case CELL_TASK_FAULTED: return "faulted";
	default: return "unknown";
	}
}
const char *cell_task_fault_name(cell_task_fault_t fault) {
	switch (fault) {
	case CELL_TASK_FAULT_NONE: return "none";
	case CELL_TASK_FAULT_LOAD: return "load";
	case CELL_TASK_FAULT_PERMISSION: return "permission";
	case CELL_TASK_FAULT_GAS: return "gas";
	case CELL_TASK_FAULT_PC: return "pc";
	case CELL_TASK_FAULT_MEMORY: return "memory";
	case CELL_TASK_FAULT_CAPABILITY: return "capability";
	case CELL_TASK_FAULT_OUTPUT: return "output";
	case CELL_TASK_FAULT_INSTRUCTION: return "instruction";
	default: return "unknown";
	}
}

static uint64_t all_known_caps(void) {
	uint64_t m = 0;
	for (unsigned id = CELL_CAP_SYSTEM_STATUS; id <= CELL_CAP_POWER_STATUS; ++id) m |= 1ull << id;
	return m;
}

uint64_t cell_task_default_policy(void) {
	return (1ull << CELL_CAP_SYSTEM_STATUS) |
		(1ull << CELL_CAP_CPU_INFO) |
		(1ull << CELL_CAP_MEMORY_STATUS) |
		(1ull << CELL_CAP_STORAGE_LIST) |
		(1ull << CELL_CAP_STORAGE_LIST_DIR) |
		(1ull << CELL_CAP_STORAGE_PWD);
}

void cell_task_manager_init(cell_task_manager_t *tm, uint64_t policy_mask) {
	if (!tm) return;
	zero_bytes(tm, sizeof(*tm));
	tm->next_id = 1u;
	tm->policy_mask = policy_mask & all_known_caps();
}

static cell_task_record_t *new_record(cell_task_manager_t *tm, const char *path) {
	uint32_t slot = tm->next_slot++ % CELL_TASK_HISTORY;
	cell_task_record_t *r = &tm->history[slot];
	zero_bytes(r, sizeof(*r));
	r->id = tm->next_id++;
	if (!tm->next_id) tm->next_id = 1u;
	r->state = CELL_TASK_READY;
	copy_text(r->path, sizeof(r->path), path);
	return r;
}

static int output_bytes(outbuf_t *b, const uint8_t *p, uint32_t n) {
	for (uint32_t i = 0; i < n; ++i) {
		uint8_t c = p[i];
		if (!((c >= 32u && c <= 126u) || c == '\n' || c == '\r' || c == '\t')) return 0;
		ob_char(b, (char)c);
		if (!b->ok) return 0;
	}
	return 1;
}

static void fault(cell_task_record_t *r, cell_task_fault_t why) {
	r->state = CELL_TASK_FAULTED;
	r->fault = why;
	r->exit_code = -1;
}

static int append_capability(outbuf_t *b, const cell_capability_env_t *env,
	cell_capability_id_t id, uint64_t *ok_value) {
	cell_capability_result_t result;
	char text[CELL_CAP_RESPONSE_MAX];
	if (!cell_capability_execute(env, id, &result)) return 0;
	if (!cell_capability_human_response(&result, text, sizeof(text))) return 0;
	ob_str(b, text);
	if (!b->ok) return 0;
	if (ok_value) *ok_value = result.ok ? 1u : 0u;
	return 1;
}

int cell_task_run(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const char *path,
	char *out, size_t cap, uint32_t *task_id) {
	if (!tm || !vfs || !env || !path || !*path || !out || cap < 2u) return 0;
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute)) return 0;
	cell_task_record_t *r = new_record(tm, absolute);
	if (task_id) *task_id = r->id;
	outbuf_t b; ob_init(&b, out, cap);
	if (!(absolute[0]=='/' && absolute[1]=='p' && absolute[2]=='r' && absolute[3]=='o' &&
	      absolute[4]=='g' && absolute[5]=='r' && absolute[6]=='a' && absolute[7]=='m' &&
	      absolute[8]=='s' && absolute[9]=='/')) {
		fault(r, CELL_TASK_FAULT_PERMISSION);
		ob_str(&b, absolute); ob_str(&b, ": Permission denied.");
		return b.ok;
	}

	size_t image_bytes = 0;
	cell_vfs_status_t vs = cell_vfs_read_bytes(vfs, absolute, tm->image, sizeof(tm->image), &image_bytes);
	if (vs != CELL_VFS_OK) {
		fault(r, CELL_TASK_FAULT_LOAD);
		ob_str(&b, absolute); ob_str(&b, vs == CELL_VFS_NOT_FOUND ? ": No such file or directory." : ": I/O error.");
		return b.ok;
	}
	cell_exec_t exec;
	cell_exec_status_t es = cell_exec_open(&exec, tm->image, image_bytes, all_known_caps());
	if (es != CELL_EXEC_OK) {
		fault(r, CELL_TASK_FAULT_LOAD);
		(void)es;
		ob_str(&b, absolute); ob_str(&b, ": Exec format error.");
		return b.ok;
	}
	r->declared_caps = exec.h->capability_mask;
	if (r->declared_caps & ~tm->policy_mask) {
		fault(r, CELL_TASK_FAULT_PERMISSION);
		ob_str(&b, absolute); ob_str(&b, ": Permission denied.");
		return b.ok;
	}
	r->granted_caps = r->declared_caps;
	zero_bytes(tm->regs, sizeof(tm->regs));
	zero_bytes(tm->memory, sizeof(tm->memory));
	r->pc = exec.h->entry_pc;
	r->state = CELL_TASK_RUNNING;

	for (;;) {
		if (r->gas_used >= exec.h->gas_limit) {
			fault(r, CELL_TASK_FAULT_GAS);
			break;
		}
		if (r->pc >= exec.instruction_count) {
			fault(r, CELL_TASK_FAULT_PC);
			break;
		}
		const cell_exec_insn_t *in = &exec.code[r->pc];
		uint32_t next_pc = r->pc + 1u;
		++r->gas_used;
		switch (in->opcode) {
		case CELL_EXEC_OP_HALT:
			r->exit_code = in->imm;
			r->state = CELL_TASK_EXITED;
			r->fault = CELL_TASK_FAULT_NONE;
			break;
		case CELL_EXEC_OP_MOVI:
			tm->regs[in->dst] = (uint64_t)(int64_t)in->imm;
			break;
		case CELL_EXEC_OP_MOV:
			tm->regs[in->dst] = tm->regs[in->a];
			break;
		case CELL_EXEC_OP_ADD:
			tm->regs[in->dst] = tm->regs[in->a] + tm->regs[in->b];
			break;
		case CELL_EXEC_OP_ADDI:
			tm->regs[in->dst] = tm->regs[in->a] + (uint64_t)(int64_t)in->imm;
			break;
		case CELL_EXEC_OP_SUB:
			tm->regs[in->dst] = tm->regs[in->a] - tm->regs[in->b];
			break;
		case CELL_EXEC_OP_MUL:
			tm->regs[in->dst] = tm->regs[in->a] * tm->regs[in->b];
			break;
		case CELL_EXEC_OP_JZ:
			if (tm->regs[in->a] == 0u) next_pc = (uint32_t)((int64_t)r->pc + 1 + in->imm);
			break;
		case CELL_EXEC_OP_JNZ:
			if (tm->regs[in->a] != 0u) next_pc = (uint32_t)((int64_t)r->pc + 1 + in->imm);
			break;
		case CELL_EXEC_OP_JMP:
			next_pc = (uint32_t)((int64_t)r->pc + 1 + in->imm);
			break;
		case CELL_EXEC_OP_PUTS: {
			uint32_t len = (uint32_t)in->a | ((uint32_t)in->b << 8);
			if (!output_bytes(&b, exec.data + (uint32_t)in->imm, len)) fault(r, CELL_TASK_FAULT_OUTPUT);
			break;
		}
		case CELL_EXEC_OP_CAP: {
			cell_capability_id_t id = (cell_capability_id_t)in->imm;
			uint64_t bit = 1ull << (uint32_t)id;
			if (!(r->granted_caps & bit)) { fault(r, CELL_TASK_FAULT_PERMISSION); break; }
			if (!append_capability(&b, env, id, &tm->regs[in->dst])) fault(r, CELL_TASK_FAULT_CAPABILITY);
			break;
		}
		case CELL_EXEC_OP_LOAD8: {
			int64_t addr = (int64_t)tm->regs[in->a] + (int64_t)in->imm;
			if (addr < 0 || (uint64_t)addr >= exec.h->memory_bytes) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->regs[in->dst] = tm->memory[(uint32_t)addr];
			break;
		}
		case CELL_EXEC_OP_STORE8: {
			int64_t addr = (int64_t)tm->regs[in->a] + (int64_t)in->imm;
			if (addr < 0 || (uint64_t)addr >= exec.h->memory_bytes) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->memory[(uint32_t)addr] = (uint8_t)tm->regs[in->b];
			break;
		}
		default:
			fault(r, CELL_TASK_FAULT_INSTRUCTION);
			break;
		}
		if (r->state == CELL_TASK_EXITED || r->state == CELL_TASK_FAULTED) break;
		r->pc = next_pc;
	}

	if (r->state == CELL_TASK_FAULTED) {
		if (b.p != out) ob_char(&b, '\n');
		ob_str(&b, absolute); ob_str(&b, ": runtime fault: ");
		ob_str(&b, cell_task_fault_name(r->fault)); ob_char(&b, '.');
	}
	return b.ok;
}

static const cell_task_record_t *find_task(const cell_task_manager_t *tm, uint32_t id) {
	if (!tm || !id) return 0;
	for (uint32_t i = 0; i < CELL_TASK_HISTORY; ++i)
		if (tm->history[i].state != CELL_TASK_EMPTY && tm->history[i].id == id) return &tm->history[i];
	return 0;
}

static void describe_record(outbuf_t *b, const cell_task_record_t *r) {
	ob_u32(b, r->id); ob_char(b, ' '); ob_str(b, cell_task_state_name(r->state)); ob_char(b, ' ');
	if (r->state == CELL_TASK_EXITED) ob_i32(b, r->exit_code);
	else ob_char(b, '-');
	ob_char(b, ' '); ob_u32(b, r->gas_used); ob_char(b, ' '); ob_str(b, r->path);
	if (r->state == CELL_TASK_FAULTED) { ob_str(b, " ["); ob_str(b, cell_task_fault_name(r->fault)); ob_char(b, ']'); }
}

static void ps_header(outbuf_t *b) {
	ob_str(b, "PID STATE EXIT GAS COMMAND");
}

int cell_task_list(const cell_task_manager_t *tm, char *out, size_t cap) {
	if (!tm || !out || !cap) return 0;
	outbuf_t b; ob_init(&b, out, cap);
	ps_header(&b);
	uint32_t min_id = tm->next_id > CELL_TASK_HISTORY ? tm->next_id - CELL_TASK_HISTORY : 1u;
	for (uint32_t id = min_id; id < tm->next_id; ++id) {
		const cell_task_record_t *r = find_task(tm, id);
		if (!r) continue;
		ob_char(&b, '\n'); describe_record(&b, r);
	}
	return b.ok;
}

int cell_task_describe(const cell_task_manager_t *tm, uint32_t id, char *out, size_t cap) {
	if (!tm || !out || !cap) return 0;
	const cell_task_record_t *r = find_task(tm, id);
	if (!r) { copy_text(out, cap, "ps: process not found."); return 1; }
	outbuf_t b; ob_init(&b, out, cap); ps_header(&b); ob_char(&b, '\n'); describe_record(&b, r); return b.ok;
}
