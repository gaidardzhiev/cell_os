/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "boot/boot_ext.h"
#include "core/handoff.h"
#include "core/mem_arena.h"

#define CELL_CAP_RESULT_MAX 160u
#define CELL_CAP_RESPONSE_MAX 512u

typedef enum {
	CELL_CAP_INVALID = 0,
	CELL_CAP_SYSTEM_STATUS,
	CELL_CAP_CPU_INFO,
	CELL_CAP_MEMORY_STATUS,
	CELL_CAP_STORAGE_LIST,
	CELL_CAP_STORAGE_LIST_DIR,
	CELL_CAP_STORAGE_PWD,
	CELL_CAP_NETWORK_LIST,
	CELL_CAP_GPU_INFO,
	CELL_CAP_USB_LIST,
	CELL_CAP_DISPLAY_INFO,
	CELL_CAP_POWER_STATUS
} cell_capability_id_t;

typedef struct cell_vfs cell_vfs_t;
typedef struct cell_task_manager cell_task_manager_t;

typedef struct cell_capability_env {
	const handoff_t *handoff;
	const cell_boot_ext_t *boot_ext;
	const cell_mem_arena_t *arena;
	int cortex_ready;
	int ata0_ready;
	cell_vfs_t *vfs;
	cell_task_manager_t *tasks;
} cell_capability_env_t;

typedef struct {
	cell_capability_id_t id;
	int ok;
	char text[CELL_CAP_RESULT_MAX];
	uint64_t top_mib;
	uint64_t usable_mib;
	uint64_t free_mib;
	uint64_t memory_mib;
	uint32_t family;
	uint32_t model;
	uint32_t logical;
	uint32_t count;
	char vendor[13];
	char cortex_state[12];
	char storage[12];
	char primary[12];
	char vfs_text[384];
} cell_capability_result_t;

const char *cell_capability_name(cell_capability_id_t id);
int cell_capability_parse_call(const char *text, size_t len, cell_capability_id_t *id);
int cell_capability_execute(const cell_capability_env_t *env, cell_capability_id_t id,
	cell_capability_result_t *result);
int cell_capability_human_response(const cell_capability_result_t *result,
	char *out, size_t out_bytes);
