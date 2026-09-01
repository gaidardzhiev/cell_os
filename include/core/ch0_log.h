/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

int ch0_write_parcel(const uint8_t* buf, size_t len);
void ch0_log_line(const char* s);
