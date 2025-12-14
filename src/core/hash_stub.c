/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/hash.h"

void cell_hash32_stub(const uint8_t* buf, size_t len, uint8_t out[32]) {
	uint32_t s=0xC0DEFACEu;
	for (size_t i=0;i<len;i++) s = (s<<5) ^ (s>>2) ^ buf[i];
	for (int i=0;i<32;i++) out[i] = (uint8_t)((s >> ((i&3)*8)) & 0xFF);
}
