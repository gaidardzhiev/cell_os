/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define CELL_CC_SOURCE_MAX 8192u
#define CELL_CC_UNIT_MAX 8u
#define CELL_CC_MAX_CODE 1024u
#define CELL_CC_MAX_DATA 8192u
#define CELL_CC_DIAG_MAX 192u
#define CELL_CC_TASK_MEMORY 4096u
#define CELL_CC_STATIC_MEMORY 1024u

typedef enum {
	CELL_CC_OK = 0,
	CELL_CC_BAD_ARGUMENT,
	CELL_CC_SOURCE_TOO_LARGE,
	CELL_CC_LEX_ERROR,
	CELL_CC_PARSE_ERROR,
	CELL_CC_TOO_MANY_VARIABLES,
	CELL_CC_TOO_COMPLEX,
	CELL_CC_OUTPUT_TOO_LARGE,
	CELL_CC_UNKNOWN_CAPABILITY
} cell_cc_status_t;
typedef struct {
	cell_cc_status_t status;
	uint32_t line;
	uint32_t column;
	char message[CELL_CC_DIAG_MAX];
} cell_cc_diag_t;
typedef struct {
	const char *name;
	const char *source;
	size_t source_bytes;
} cell_cc_source_unit_t;

const char *cell_cc_status_name(cell_cc_status_t status);
int cell_cc_compile(const char *source, size_t source_bytes,
	void *output, size_t output_cap, size_t *output_bytes, cell_cc_diag_t *diag);
int cell_cc_compile_units(const cell_cc_source_unit_t *units, size_t unit_count,
	void *output, size_t output_cap, size_t *output_bytes, cell_cc_diag_t *diag);
