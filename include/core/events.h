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

typedef enum {
	EV_NONE     = 0,
	EV_TIMER    = 1,
	EV_BLOCK_IO = 2,
	EV_SERIAL_RX= 3,
	EV_NET_RX   = 4,
	EV_CUSTOM   = 16
} event_kind_t;

typedef struct __attribute__((packed)) {
	uint32_t seq;
	uint32_t src;
	uint32_t kind;
	uint32_t len;
} event_hdr_t;

typedef struct {
	uint8_t buf[256];
	size_t  len;
} event_frame_t;

bool event_pack(event_frame_t* out, uint32_t seq, uint32_t src, event_kind_t kind, const void* payload, uint32_t len);
