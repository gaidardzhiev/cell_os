/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

void cell_hash32_stub(const uint8_t* buf, size_t len, uint8_t out[32]);

void cell_sha256(const uint8_t* buf, size_t len, uint8_t out[32]);

#if defined(BUILD_STRICT_CRYPTO)
#  define cell_hash  cell_sha256
#else
#  define cell_hash  cell_hash32_stub
#endif
