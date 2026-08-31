#pragma once
#include <stddef.h>
#include <stdint.h>

int ata_pio_read28(uint32_t lba, uint8_t *dst, uint32_t sectors);
int ata_pio_read_bytes(uint64_t lba, void *dst, uint64_t bytes);
