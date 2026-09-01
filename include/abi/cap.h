/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stdint.h>

#define CAP_ABI_MAJOR 1
#define CAP_ABI_MINOR 0

typedef struct __attribute__((packed)) {
	uint32_t channel_id;
	uint32_t verbs_mask;
	uint32_t version;
	uint64_t quota_cpu;
	uint64_t quota_io;
	uint64_t quota_mem;
	uint8_t  tag[16];
} cap_token_t;
