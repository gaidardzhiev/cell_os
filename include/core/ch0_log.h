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

int ch0_write_parcel(const uint8_t* buf, size_t len);
void ch0_log_line(const char* s);
