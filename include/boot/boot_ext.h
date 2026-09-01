/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#pragma once
#include <stdint.h>

#define CELL_BOOT_EXT_MAGIC 0x31584243u /* CBX1 */
#define CELL_BOOT_EXT_VERSION 1u
#define CELL_BOOT_EXT_F_MODEL 0x00000001u
#define CELL_BOOT_EXT_F_CELLFS 0x00000002u

typedef struct __attribute__((packed)) {
	uint64_t base;
	uint64_t length;
	uint32_t type;
	uint32_t attrs;
} cell_e820_entry_t;

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t size;
	uint64_t e820_ptr;
	uint32_t e820_count;
	uint32_t e820_entry_size;
	uint64_t model_lba;
	uint64_t model_bytes;
	uint32_t flags;
	uint32_t cellfs_sectors;
} cell_boot_ext_t;

_Static_assert(sizeof(cell_e820_entry_t) == 24, "E820 ABI");
_Static_assert(sizeof(cell_boot_ext_t) == 48, "boot extension ABI");
