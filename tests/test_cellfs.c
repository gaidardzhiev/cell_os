/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/cellfs.h"

#define SECTORS 1024u

static uint8_t disk_bytes[SECTORS * CELLFS_SECTOR_SIZE];

static int mem_read(void *ctx, uint64_t lba, void *dst, uint32_t sectors) {
	(void)ctx;
	if (!dst || lba >= SECTORS || sectors > SECTORS - lba) return 0;
	memcpy(dst, disk_bytes + (size_t)lba * CELLFS_SECTOR_SIZE,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}

static int mem_write(void *ctx, uint64_t lba, const void *src, uint32_t sectors) {
	(void)ctx;
	if (!src || lba >= SECTORS || sectors > SECTORS - lba) return 0;
	memcpy(disk_bytes + (size_t)lba * CELLFS_SECTOR_SIZE, src,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}

static int expect(int cond, const char *msg) {
	if (cond) return 1;
	fprintf(stderr, "FAIL: %s\n", msg);
	return 0;
}

int main(void) {
	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {
		.read = mem_read,
		.write = mem_write,
		.ctx = 0,
		.base_lba = 0,
		.total_sectors = SECTORS
	};
	cellfs_t *fs = calloc(1, sizeof(*fs));
	cellfs_t *fs2 = calloc(1, sizeof(*fs2));
	if (!fs || !fs2) return 2;
	int ok = 1;
	ok &= expect(cellfs_format(fs, &disk), "format");
	uint32_t home = 0;
	ok &= expect(cellfs_find_child(fs, 1, "home", &home) && home == 2, "home inode");
	uint32_t notes = 0;
	ok &= expect(cellfs_create_dir(fs, home, "notes", &notes), "mkdir notes");
	uint32_t file = 0;
	ok &= expect(cellfs_create_file(fs, notes, "hello.txt", &file), "create hello.txt");
	const char *hello = "hello from CellFS";
	ok &= expect(cellfs_write_file(fs, file, hello, strlen(hello), 0), "write hello");
	const char *tail = " + persistent";
	ok &= expect(cellfs_write_file(fs, file, tail, strlen(tail), 1), "append hello");
	char out[128];
	size_t n = 0;
	ok &= expect(cellfs_read_file(fs, file, out, sizeof(out) - 1u, &n), "read hello");
	out[n] = 0;
	ok &= expect(strcmp(out, "hello from CellFS + persistent") == 0, "content before remount");

	ok &= expect(cellfs_mount(fs2, &disk), "remount");
	uint32_t notes2 = 0, file2 = 0;
	ok &= expect(cellfs_find_child(fs2, 2, "notes", &notes2), "find notes after remount");
	ok &= expect(cellfs_find_child(fs2, notes2, "hello.txt", &file2), "find file after remount");
	n = 0;
	ok &= expect(cellfs_read_file(fs2, file2, out, sizeof(out) - 1u, &n), "read after remount");
	out[n] = 0;
	ok &= expect(strcmp(out, "hello from CellFS + persistent") == 0, "persistent content exact");

	uint8_t saved = disk_bytes[31];
	disk_bytes[31] ^= 0x5Au;
	cellfs_t *bad = calloc(1, sizeof(*bad));
	ok &= expect(bad && !cellfs_mount(bad, &disk), "reject corrupt superblock CRC");
	disk_bytes[31] = saved;
	free(bad);

	ok &= expect(cellfs_remove(fs2, file2, 0), "remove file");
	ok &= expect(!cellfs_find_child(fs2, notes2, "hello.txt", &file2), "removed file absent");
	ok &= expect(cellfs_remove(fs2, notes2, 1), "remove empty directory");

	free(fs2);
	free(fs);
	if (!ok) return 1;
	puts("#CELLFS format/mount PASS");
	puts("#CELLFS persistent remount PASS");
	puts("#CELLFS CRC rejection PASS");
	puts("#CELLFS create/write/append/remove PASS");
	return 0;
}
