/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>

void dbg_puts(const char* s);
void dbg_hex32(uint32_t v);
void dbg_hex64(uint64_t v);
