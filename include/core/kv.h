/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int kv_init(void);
int kv_put(const char* ns, const char* key, const void* p, size_t n);
int kv_get(const char* ns, const char* key, void* p, size_t* n_inout);
int kv_flush(void);
uint32_t kv_crc32(void);

#define KV_OK 0
#define KV_ERR (-1)
