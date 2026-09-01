/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/vfs.h"
#include "core/capability.h"

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
static int expect(int cond, const char *msg, const char *got) {
	if (cond) return 1;
	fprintf(stderr, "FAIL: %s", msg);
	if (got) fprintf(stderr, "\n  got: %s", got);
	fputc('\n', stderr);
	return 0;
}

int main(void) {
	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {mem_read, mem_write, 0, 0, SECTORS};
	cellfs_t *formatter = calloc(1, sizeof(*formatter));
	cell_vfs_t *vfs = calloc(1, sizeof(*vfs));
	if (!formatter || !vfs || !cellfs_format(formatter, &disk)) return 2;
	free(formatter);
	if (!cell_vfs_mount(vfs, &disk, 1054400u, 256u, 256u, 128u, 5u)) return 2;

	cell_e820_entry_t map[2] = {
		{0, 640u * 1024u, 1, 0},
		{0x100000u, 511ull * 1024u * 1024u, 1, 0}
	};
	cell_boot_ext_t ext = {0};
	ext.e820_ptr = (uint64_t)(uintptr_t)map;
	ext.e820_count = 2;
	ext.e820_entry_size = sizeof(map[0]);
	handoff_t ho = {0};
	ho.mem_top = 512ull * 1024u * 1024u;
	cell_mem_arena_t arena = {16ull * 1024u * 1024u, 512ull * 1024u * 1024u};
	cell_capability_env_t env = {
		.handoff = &ho, .boot_ext = &ext, .arena = &arena,
		.cortex_ready = 1, .ata0_ready = 1, .vfs = vfs
	};
	char out[CELL_VFS_TEXT_MAX];
	int ok = 1;
	ok &= expect(cell_vfs_list(vfs, &env, "/", out, sizeof(out)) == CELL_VFS_OK,
		"list root", out);
	ok &= expect(strcmp(out, "home/  programs/  models/  system/  devices/") == 0,
		"root namespace exact", out);
	ok &= expect(cell_vfs_chdir(vfs, "/home", out, sizeof(out)) == CELL_VFS_OK,
		"cd /home", out);
	ok &= expect(cell_vfs_pwd(vfs, out, sizeof(out)) == CELL_VFS_OK && strcmp(out, "/home") == 0,
		"pwd /home", out);
	ok &= expect(cell_vfs_mkdir(vfs, "notes") == CELL_VFS_OK, "mkdir relative", 0);
	ok &= expect(cell_vfs_write(vfs, "notes/hello.txt", "Cell OS remembers this.", 0) == CELL_VFS_OK,
		"write persistent file", 0);
	ok &= expect(cell_vfs_cat(vfs, &env, "notes/hello.txt", out, sizeof(out)) == CELL_VFS_OK,
		"cat persistent file", out);
	ok &= expect(strcmp(out, "Cell OS remembers this.") == 0, "persistent file exact", out);
	{
		char absolute[CELL_VFS_PATH_MAX]; size_t size = 0, got = 0; char range[16] = {0};
		ok &= expect(cell_vfs_open_file(vfs, &env, "notes/hello.txt", CELL_VFS_OPEN_READ | CELL_VFS_OPEN_WRITE,
			absolute, &size) == CELL_VFS_OK && strcmp(absolute, "/home/notes/hello.txt") == 0 && size == 23u,
			"open persistent file for range I/O", absolute);
		ok &= expect(cell_vfs_read_at(vfs, &env, absolute, 5u, range, 2u, &got) == CELL_VFS_OK &&
			got == 2u && memcmp(range, "OS", 2u) == 0, "persistent range read", 0);
		ok &= expect(cell_vfs_write_at(vfs, absolute, 5u, "VM", 2u, &size) == CELL_VFS_OK && size == 23u,
			"persistent range write", 0);
		ok &= expect(cell_vfs_cat(vfs, &env, absolute, out, sizeof(out)) == CELL_VFS_OK &&
			strcmp(out, "Cell VM remembers this.") == 0, "range write visible", out);
	}
	ok &= expect(cell_vfs_list(vfs, &env, "notes", out, sizeof(out)) == CELL_VFS_OK,
		"list notes", out);
	ok &= expect(strcmp(out, "hello.txt") == 0, "list file exact", out);
	ok &= expect(cell_vfs_cat(vfs, &env, "/devices/cpu", out, sizeof(out)) == CELL_VFS_OK,
		"read live CPU node", out);
	ok &= expect(strncmp(out, "CPU: ", 5) == 0, "live CPU prefix", out);
	{
		char absolute[CELL_VFS_PATH_MAX]; char range[8] = {0}; size_t size = 0, got = 0;
		ok &= expect(cell_vfs_open_file(vfs, &env, "/devices/cpu", CELL_VFS_OPEN_READ, absolute, &size) == CELL_VFS_OK && size >= 5u,
			"open live node through file interface", absolute);
		ok &= expect(cell_vfs_read_at(vfs, &env, absolute, 0u, range, 5u, &got) == CELL_VFS_OK &&
			got == 5u && memcmp(range, "CPU: ", 5u) == 0, "range read live node", 0);
		ok &= expect(cell_vfs_open_file(vfs, &env, "/devices/cpu", CELL_VFS_OPEN_WRITE, absolute, &size) == CELL_VFS_READ_ONLY,
			"live node open-for-write rejected", 0);
	}
	ok &= expect(cell_vfs_cat(vfs, &env, "/models/cortex", out, sizeof(out)) == CELL_VFS_OK,
		"read live model node", out);
	ok &= expect(strstr(out, "1054400 bytes") != 0, "model bytes live", out);
	ok &= expect(cell_vfs_write(vfs, "/system/status", "evil", 0) == CELL_VFS_READ_ONLY,
		"virtual namespace is read only", 0);
	uint32_t programs = 0, binary_id = 0;
	uint8_t binary_data[4] = {0x43, 0x45, 0x58, 0x00};
	ok &= expect(cellfs_find_child(&vfs->fs, 1, "programs", &programs), "find programs", 0);
	ok &= expect(cellfs_create_file(&vfs->fs, programs, "binary.cellx", &binary_id), "create binary", 0);
	ok &= expect(cellfs_write_file(&vfs->fs, binary_id, binary_data, sizeof(binary_data), 0), "write binary", 0);
	ok &= expect(cell_vfs_cat(vfs, &env, "/programs/binary.cellx", out, sizeof(out)) == CELL_VFS_BINARY,
		"cat refuses binary", out);
	ok &= expect(cell_vfs_chdir(vfs, "../../devices", out, sizeof(out)) == CELL_VFS_OK &&
		strcmp(out, "/devices") == 0, "normalize dotdot", out);
	ok &= expect(cell_vfs_list(vfs, &env, 0, out, sizeof(out)) == CELL_VFS_OK &&
		strstr(out, "cpu") != 0, "list current virtual dir", out);

	free(vfs);
	if (!ok) return 1;
	puts("#VFS namespace PASS");
	puts("#VFS path normalization PASS");
	puts("#VFS persistent nodes PASS");
	puts("#VFS live capability nodes PASS");
	puts("#VFS read-only virtual boundary PASS");
	puts("#VFS binary/text boundary PASS");
	puts("#VFS descriptor range I/O primitives PASS");
	return 0;
}
