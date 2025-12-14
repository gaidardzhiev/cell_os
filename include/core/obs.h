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
#include "abi/msg.h"
#include "core/parcel.h"

#define OBS_VER_MAJOR 2
#define OBS_VER_MINOR 0

typedef enum {
	OBS_REC_MARK = 1,
	OBS_REC_EVENT = 2,
	OBS_REC_SYS = 3,
	OBS_REC_TRACE= 4
} obs_rec_kind_t;

typedef struct __attribute__((packed)) {
	uint32_t ver_major, ver_minor;
	uint32_t rec_kind;
	uint32_t tick;
	uint32_t seq;
	uint32_t payload_len;
} obs_hdr_t;

bool obs_pack_to_ch0(const obs_hdr_t* h, const void* payload, uint8_t* out, size_t cap, size_t* out_len);

bool obs_mark_line(uint32_t tick, uint32_t seq, const char* line, uint8_t* out, size_t cap, size_t* out_len);
