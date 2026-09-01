/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/cellexec.h"

#define CELL_TASK_HISTORY 8u
#define CELL_TASK_PATH_MAX 128u
#define CELL_TASK_OUTPUT_MAX 768u

struct cell_vfs;
typedef struct cell_vfs cell_vfs_t;
struct cell_capability_env;
typedef struct cell_capability_env cell_capability_env_t;

typedef enum {
	CELL_TASK_EMPTY = 0,
	CELL_TASK_READY,
	CELL_TASK_RUNNING,
	CELL_TASK_EXITED,
	CELL_TASK_FAULTED
} cell_task_state_t;

typedef enum {
	CELL_TASK_FAULT_NONE = 0,
	CELL_TASK_FAULT_LOAD,
	CELL_TASK_FAULT_PERMISSION,
	CELL_TASK_FAULT_GAS,
	CELL_TASK_FAULT_PC,
	CELL_TASK_FAULT_MEMORY,
	CELL_TASK_FAULT_CAPABILITY,
	CELL_TASK_FAULT_OUTPUT,
	CELL_TASK_FAULT_INSTRUCTION
} cell_task_fault_t;

typedef struct {
	uint32_t id;
	cell_task_state_t state;
	cell_task_fault_t fault;
	int32_t exit_code;
	uint32_t gas_used;
	uint32_t pc;
	uint64_t declared_caps;
	uint64_t granted_caps;
	char path[CELL_TASK_PATH_MAX];
} cell_task_record_t;

typedef struct cell_task_manager {
	uint32_t next_id;
	uint32_t next_slot;
	uint64_t policy_mask;
	cell_task_record_t history[CELL_TASK_HISTORY];
	uint64_t regs[CELL_EXEC_REGS];
	uint8_t memory[CELL_EXEC_MEMORY_MAX];
	uint8_t image[CELL_EXEC_FILE_MAX];
} cell_task_manager_t;

void cell_task_manager_init(cell_task_manager_t *tm, uint64_t policy_mask);
const char *cell_task_state_name(cell_task_state_t state);
const char *cell_task_fault_name(cell_task_fault_t fault);
uint64_t cell_task_default_policy(void);
int cell_task_run(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const char *path,
	char *out, size_t cap, uint32_t *task_id);
int cell_task_list(const cell_task_manager_t *tm, char *out, size_t cap);
int cell_task_describe(const cell_task_manager_t *tm, uint32_t id, char *out, size_t cap);
