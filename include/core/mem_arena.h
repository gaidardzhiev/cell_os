/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/handoff.h"

typedef struct {
	uintptr_t next;
	uintptr_t end;
} cell_mem_arena_t;

int cell_mem_arena_init(cell_mem_arena_t *arena, const handoff_t *ho);
void *cell_mem_alloc(cell_mem_arena_t *arena, size_t bytes, size_t alignment);
size_t cell_mem_available(const cell_mem_arena_t *arena);
