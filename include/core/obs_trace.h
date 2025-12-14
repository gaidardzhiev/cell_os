/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
	uint64_t entries;
	uint64_t marks;
	uint64_t events;
	uint64_t sys;
	uint64_t traces;
	uint8_t hash32[32];
} obs_index_t;

void obs_index_init(obs_index_t* i);
void obs_index_feed(obs_index_t* i, const uint8_t* blob, size_t len);
