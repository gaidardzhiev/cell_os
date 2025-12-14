/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>

#define MSG_ABI_MAJOR 1
#define MSG_ABI_MINOR 0

typedef struct __attribute__((packed)) {
	uint32_t channel_id;
	uint32_t verb;
	uint32_t flags;
	uint32_t len;
} msg_t;
