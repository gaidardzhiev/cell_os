/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "drivers/ata_pio.h"

#define ATA_DATA 0x1F0
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7
#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_FLUSH 0xE7
#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF  0x20
#define ATA_SR_BSY 0x80

static inline void outb(uint16_t port, uint8_t v) {
	__asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
	uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port)); return v;
}
static inline uint16_t inw(uint16_t port) {
	uint16_t v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(port)); return v;
}
static inline void outw(uint16_t port, uint16_t v) {
	__asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(port));
}

static int ata_wait(void) {
	for (uint32_t spin = 0; spin < 10000000u; ++spin) {
		uint8_t s = inb(ATA_STATUS);
		if (s & (ATA_SR_ERR | ATA_SR_DF)) return 0;
		if (!(s & ATA_SR_BSY) && (s & ATA_SR_DRQ)) return 1;
	}
	return 0;
}

int ata_pio_read28(uint32_t lba, uint8_t *dst, uint32_t sectors) {
	if (!dst || !sectors || lba > 0x0FFFFFFFu) return 0;
	while (sectors) {
		uint8_t count = sectors > 255u ? 255u : (uint8_t)sectors;
		outb(ATA_DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
		outb(ATA_SECCOUNT, count);
		outb(ATA_LBA0, (uint8_t)lba);
		outb(ATA_LBA1, (uint8_t)(lba >> 8));
		outb(ATA_LBA2, (uint8_t)(lba >> 16));
		outb(ATA_COMMAND, ATA_CMD_READ);
		for (uint32_t s = 0; s < count; ++s) {
			if (!ata_wait()) return 0;
			for (uint32_t i = 0; i < 256; ++i) {
				uint16_t w = inw(ATA_DATA);
				*dst++ = (uint8_t)w;
				*dst++ = (uint8_t)(w >> 8);
			}
		}
		lba += count;
		sectors -= count;
	}
	return 1;
}

int ata_pio_read_bytes(uint64_t lba, void *dst, uint64_t bytes) {
	if (!dst || !bytes || lba > 0x0FFFFFFFu) return 0;
	uint64_t sectors64 = (bytes + 511u) / 512u;
	if (sectors64 > 0x0FFFFFFFu) return 0;
	return ata_pio_read28((uint32_t)lba, (uint8_t *)dst, (uint32_t)sectors64);
}


static int ata_wait_not_busy(void) {
	for (uint32_t spin = 0; spin < 10000000u; ++spin) {
		uint8_t s = inb(ATA_STATUS);
		if (s & (ATA_SR_ERR | ATA_SR_DF)) return 0;
		if (!(s & ATA_SR_BSY)) return 1;
	}
	return 0;
}

int ata_pio_write28(uint32_t lba, const uint8_t *src, uint32_t sectors) {
	if (!src || !sectors || lba > 0x0FFFFFFFu) return 0;
	while (sectors) {
		uint8_t count = sectors > 255u ? 255u : (uint8_t)sectors;
		outb(ATA_DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
		outb(ATA_SECCOUNT, count);
		outb(ATA_LBA0, (uint8_t)lba);
		outb(ATA_LBA1, (uint8_t)(lba >> 8));
		outb(ATA_LBA2, (uint8_t)(lba >> 16));
		outb(ATA_COMMAND, ATA_CMD_WRITE);
		for (uint32_t s = 0; s < count; ++s) {
			if (!ata_wait()) return 0;
			for (uint32_t i = 0; i < 256u; ++i) {
				uint16_t w = (uint16_t)src[0] | ((uint16_t)src[1] << 8);
				outw(ATA_DATA, w);
				src += 2;
			}
		}
		outb(ATA_COMMAND, ATA_CMD_FLUSH);
		if (!ata_wait_not_busy()) return 0;
		lba += count;
		sectors -= count;
	}
	return 1;
}

int ata_pio_write_bytes(uint64_t lba, const void *src, uint64_t bytes) {
	if (!src || !bytes || lba > 0x0FFFFFFFu || (bytes & 511u)) return 0;
	uint64_t sectors64 = bytes / 512u;
	if (!sectors64 || sectors64 > 0x0FFFFFFFu) return 0;
	return ata_pio_write28((uint32_t)lba, (const uint8_t *)src, (uint32_t)sectors64);
}
