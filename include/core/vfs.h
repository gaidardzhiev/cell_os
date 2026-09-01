/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/cellfs.h"

#define CELL_VFS_PATH_MAX 128u
#define CELL_VFS_TEXT_MAX 512u

typedef struct cell_capability_env cell_capability_env_t;

typedef enum {
	CELL_VFS_OK = 0,
	CELL_VFS_NOT_FOUND,
	CELL_VFS_NOT_DIR,
	CELL_VFS_IS_DIR,
	CELL_VFS_READ_ONLY,
	CELL_VFS_EXISTS,
	CELL_VFS_FULL,
	CELL_VFS_INVALID,
	CELL_VFS_IO,
	CELL_VFS_NOT_EMPTY,
	CELL_VFS_BUSY,
	CELL_VFS_TOO_LARGE,
	CELL_VFS_BINARY
} cell_vfs_status_t;

typedef struct cell_vfs {
	cellfs_t fs;
	char cwd[CELL_VFS_PATH_MAX];
	uint64_t model_bytes;
	uint32_t model_vocab;
	uint32_t model_context;
	uint32_t model_d_model;
	uint32_t model_layers;
	int mounted;
} cell_vfs_t;

int cell_vfs_mount(cell_vfs_t *vfs, const cellfs_disk_t *disk,
	uint64_t model_bytes, uint32_t vocab, uint32_t context, uint32_t d_model, uint32_t layers);
const char *cell_vfs_status_name(cell_vfs_status_t status);
int cell_vfs_normalize(const cell_vfs_t *vfs, const char *path, char out[CELL_VFS_PATH_MAX]);
cell_vfs_status_t cell_vfs_pwd(const cell_vfs_t *vfs, char *out, size_t cap);
cell_vfs_status_t cell_vfs_list(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap);
cell_vfs_status_t cell_vfs_chdir(cell_vfs_t *vfs, const char *path, char *out, size_t cap);
cell_vfs_status_t cell_vfs_cat(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap);
cell_vfs_status_t cell_vfs_read_bytes(cell_vfs_t *vfs, const char *path,
	void *dst, size_t cap, size_t *bytes_out);
cell_vfs_status_t cell_vfs_mkdir(cell_vfs_t *vfs, const char *path);
cell_vfs_status_t cell_vfs_touch(cell_vfs_t *vfs, const char *path);
cell_vfs_status_t cell_vfs_write(cell_vfs_t *vfs, const char *path, const char *text, int append);
cell_vfs_status_t cell_vfs_write_bytes(cell_vfs_t *vfs, const char *path,
	const void *data, size_t bytes, int append);
cell_vfs_status_t cell_vfs_remove(cell_vfs_t *vfs, const char *path, int directory);
cell_vfs_status_t cell_vfs_stat(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap);
