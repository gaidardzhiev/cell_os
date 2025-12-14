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

typedef struct {
	uint32_t crc32c;
	uint32_t genedir_off;
	uint32_t genedir_len;
	uint8_t  reserved[4096 - 12];
} cgf2_header_t;

int cgf2_verify_and_log(const uint8_t* base, size_t len);
