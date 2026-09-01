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

int ata_pio_read28(uint32_t lba, uint8_t *dst, uint32_t sectors);
int ata_pio_read_bytes(uint64_t lba, void *dst, uint64_t bytes);
int ata_pio_write28(uint32_t lba, const uint8_t *src, uint32_t sectors);
int ata_pio_write_bytes(uint64_t lba, const void *src, uint64_t bytes);
