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

#define CELLFS_MAGIC 0x31534643u /* CFS1 */
#define CELLFS_VERSION 1u
#define CELLFS_SECTOR_SIZE 512u
#define CELLFS_MAX_INODES 64u
#define CELLFS_INODE_BYTES 128u
#define CELLFS_INODE_TABLE_SECTORS ((CELLFS_MAX_INODES * CELLFS_INODE_BYTES) / CELLFS_SECTOR_SIZE)
#define CELLFS_DATA_LBA (1u + CELLFS_INODE_TABLE_SECTORS)
#define CELLFS_NAME_MAX 63u
#define CELLFS_FILE_MAX (16u * 1024u)

#define CELLFS_TYPE_FREE 0u
#define CELLFS_TYPE_DIR  1u
#define CELLFS_TYPE_FILE 2u

typedef int (*cellfs_read_fn)(void *ctx, uint64_t lba, void *dst, uint32_t sectors);
typedef int (*cellfs_write_fn)(void *ctx, uint64_t lba, const void *src, uint32_t sectors);

typedef struct {
	cellfs_read_fn read;
	cellfs_write_fn write;
	void *ctx;
	uint64_t base_lba;
	uint32_t total_sectors;
} cellfs_disk_t;

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t header_bytes;
	uint32_t sector_size;
	uint32_t total_sectors;
	uint32_t inode_count;
	uint32_t inode_entry_bytes;
	uint32_t inode_table_lba;
	uint32_t inode_table_sectors;
	uint32_t data_lba;
	uint32_t next_data_lba;
	uint32_t generation;
	uint32_t flags;
	uint32_t crc32;
	uint8_t reserved[460];
} cellfs_super_t;

typedef struct __attribute__((packed)) {
	uint32_t id;
	uint32_t parent;
	uint8_t type;
	uint8_t flags;
	uint16_t reserved0;
	uint32_t size;
	uint32_t data_lba;
	uint32_t data_sectors;
	uint32_t generation;
	char name[64];
	uint8_t reserved1[32];
	uint32_t crc32;
} cellfs_inode_t;

typedef struct {
	cellfs_disk_t disk;
	cellfs_super_t super;
	cellfs_inode_t inodes[CELLFS_MAX_INODES];
	uint8_t scratch[CELLFS_FILE_MAX];
	int mounted;
} cellfs_t;

_Static_assert(sizeof(cellfs_super_t) == CELLFS_SECTOR_SIZE, "CellFS superblock ABI");
_Static_assert(sizeof(cellfs_inode_t) == CELLFS_INODE_BYTES, "CellFS inode ABI");

uint32_t cellfs_crc32(const void *data, size_t bytes);
int cellfs_mount(cellfs_t *fs, const cellfs_disk_t *disk);
int cellfs_format(cellfs_t *fs, const cellfs_disk_t *disk);
int cellfs_find_child(const cellfs_t *fs, uint32_t parent, const char *name, uint32_t *inode_id);
const cellfs_inode_t *cellfs_inode(const cellfs_t *fs, uint32_t inode_id);
int cellfs_create_dir(cellfs_t *fs, uint32_t parent, const char *name, uint32_t *inode_id);
int cellfs_create_file(cellfs_t *fs, uint32_t parent, const char *name, uint32_t *inode_id);
int cellfs_write_file(cellfs_t *fs, uint32_t inode_id, const void *data, size_t bytes, int append);
int cellfs_read_file(const cellfs_t *fs, uint32_t inode_id, void *dst, size_t cap, size_t *bytes_out);
int cellfs_remove(cellfs_t *fs, uint32_t inode_id, int require_dir);
int cellfs_sync(cellfs_t *fs);
