/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

struct cell_capability_env;
typedef struct cell_capability_env cell_capability_env_t;

typedef enum {
	CELL_SHELL_NOT_HANDLED = 0,
	CELL_SHELL_VFS = 1,
	CELL_SHELL_PROCESS = 2
} cell_shell_result_t;

cell_shell_result_t cell_shell_execute(const char *line,
	const cell_capability_env_t *env, char *out, size_t cap, uint32_t *pid);
