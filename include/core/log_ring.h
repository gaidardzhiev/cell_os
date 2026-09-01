/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint8_t* buf;
	size_t cap;
	size_t head;
	size_t tail;
	bool full;
	uint32_t drops;
} log_ring_t;

bool log_ring_init(log_ring_t* r, uint8_t* storage, size_t cap);
bool log_ring_enqueue(log_ring_t* r, const uint8_t* data, size_t len);
size_t log_ring_drain(log_ring_t* r, int (*writer)(const uint8_t*, size_t));
uint32_t log_ring_drops(const log_ring_t* r);
