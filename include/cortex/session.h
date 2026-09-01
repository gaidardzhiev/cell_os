/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stddef.h>
#include "core/capability.h"
#include "cortex/cortex.h"

#define CORTEX_SESSION_MODEL_OUTPUT_MAX 192u
#define CORTEX_SESSION_RESPONSE_MAX 1024u
#define CORTEX_SESSION_INPUT_MAX 255u

typedef enum {
	CORTEX_SESSION_ERROR = 0,
	CORTEX_SESSION_DIRECT = 1,
	CORTEX_SESSION_CAPABILITY = 2,
	CORTEX_SESSION_VFS = 3,
	CORTEX_SESSION_TASK = 4
} cortex_session_kind_t;

typedef struct {
	cortex_session_kind_t kind;
	cell_capability_id_t capability;
	cell_capability_result_t result;
	uint32_t task_id;
	char first_pass[CORTEX_SESSION_MODEL_OUTPUT_MAX];
	char response[CORTEX_SESSION_RESPONSE_MAX];
} cortex_session_result_t;

int cortex_session_process(cortex_t *c, const char *user_text,
	const cell_capability_env_t *env, cortex_session_result_t *out);
