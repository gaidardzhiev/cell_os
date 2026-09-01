/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/cellfs.h"

static void bytes_zero(void *p, size_t n) {
	uint8_t *b = (uint8_t *)p;
	while (n--) *b++ = 0;
}

static void bytes_copy(void *dst, const void *src, size_t n) {
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	while (n--) *d++ = *s++;
}

static size_t str_len_cap(const char *s, size_t cap) {
	size_t n = 0;
	while (s && n < cap && s[n]) ++n;
	return n;
}

static int name_valid(const char *name) {
	size_t n = str_len_cap(name, CELLFS_NAME_MAX + 1u);
	if (!n || n > CELLFS_NAME_MAX) return 0;
	if (n == 1u && name[0] == '.') return 0;
	if (n == 2u && name[0] == '.' && name[1] == '.') return 0;
	for (size_t i = 0; i < n; ++i) {
		unsigned char c = (unsigned char)name[i];
		if (c == '/' || c < 32u || c > 126u) return 0;
	}
	return 1;
}

static int str_eq(const char *a, const char *b) {
	if (!a || !b) return 0;
	while (*a && *b) { if (*a++ != *b++) return 0; }
	return *a == 0 && *b == 0;
}

uint32_t cellfs_crc32(const void *data, size_t bytes) {
	const uint8_t *p = (const uint8_t *)data;
	uint32_t crc = 0xFFFFFFFFu;
	while (bytes--) {
		crc ^= *p++;
		for (unsigned i = 0; i < 8u; ++i)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

static uint32_t super_crc(const cellfs_super_t *s) {
	cellfs_super_t t = *s;
	t.crc32 = 0;
	return cellfs_crc32(&t, sizeof(t));
}

static uint32_t inode_crc(const cellfs_inode_t *ino) {
	return cellfs_crc32(ino, offsetof(cellfs_inode_t, crc32));
}

static int inode_name_terminated(const cellfs_inode_t *ino) {
	for (size_t i = 0; i < sizeof(ino->name); ++i)
		if (!ino->name[i]) return 1;
	return 0;
}

static int inode_table_valid(const cellfs_t *fs) {
	if (!fs) return 0;
	for (uint32_t i = 0; i < CELLFS_MAX_INODES; ++i) {
		const cellfs_inode_t *ino = &fs->inodes[i];
		if (ino->crc32 != inode_crc(ino)) return 0;
		if (ino->type == CELLFS_TYPE_FREE) {
			if (ino->id || ino->parent || ino->size || ino->data_lba || ino->data_sectors) return 0;
			continue;
		}
		if (ino->id != i + 1u || ino->type > CELLFS_TYPE_FILE || !inode_name_terminated(ino)) return 0;
		if (!ino->parent || ino->parent > CELLFS_MAX_INODES) return 0;
		if (ino->id == 1u) {
			if (ino->parent != 1u || ino->type != CELLFS_TYPE_DIR || ino->name[0]) return 0;
		} else {
			if (!name_valid(ino->name)) return 0;
			const cellfs_inode_t *parent = &fs->inodes[ino->parent - 1u];
			if (parent->type != CELLFS_TYPE_DIR) return 0;
		}
		if (ino->type == CELLFS_TYPE_DIR) {
			if (ino->size || ino->data_lba || ino->data_sectors) return 0;
		} else {
			if (ino->size > CELLFS_FILE_MAX) return 0;
			uint32_t expected = (ino->size + CELLFS_SECTOR_SIZE - 1u) / CELLFS_SECTOR_SIZE;
			if (ino->data_sectors != expected) return 0;
			if (!ino->size) {
				if (ino->data_lba) return 0;
			} else {
				if (ino->data_lba < fs->super.data_lba || ino->data_lba >= fs->super.next_data_lba) return 0;
				if (ino->data_sectors > fs->disk.total_sectors - ino->data_lba) return 0;
				if (ino->data_lba + ino->data_sectors > fs->super.next_data_lba) return 0;
			}
		}
	}
	/* Seed directories are part of the CellFS-1 namespace contract. */
	if (fs->inodes[1].type != CELLFS_TYPE_DIR || fs->inodes[1].parent != 1u || !str_eq(fs->inodes[1].name, "home")) return 0;
	if (fs->inodes[2].type != CELLFS_TYPE_DIR || fs->inodes[2].parent != 1u || !str_eq(fs->inodes[2].name, "programs")) return 0;
	/* Reject duplicate names under one parent and parent cycles. */
	for (uint32_t i = 1u; i < CELLFS_MAX_INODES; ++i) {
		const cellfs_inode_t *a = &fs->inodes[i];
		if (a->type == CELLFS_TYPE_FREE) continue;
		for (uint32_t j = i + 1u; j < CELLFS_MAX_INODES; ++j) {
			const cellfs_inode_t *b = &fs->inodes[j];
			if (b->type != CELLFS_TYPE_FREE && a->parent == b->parent && str_eq(a->name, b->name)) return 0;
		}
		uint32_t cursor = a->id;
		for (uint32_t depth = 0; depth < CELLFS_MAX_INODES; ++depth) {
			if (cursor == 1u) break;
			const cellfs_inode_t *node = &fs->inodes[cursor - 1u];
			cursor = node->parent;
			if (!cursor || cursor > CELLFS_MAX_INODES) return 0;
			if (depth + 1u == CELLFS_MAX_INODES) return 0;
		}
	}
	return 1;
}

static int disk_read(const cellfs_t *fs, uint32_t rel_lba, void *dst, uint32_t sectors) {
	if (!fs || !fs->disk.read || !dst || !sectors) return 0;
	if (rel_lba >= fs->disk.total_sectors || sectors > fs->disk.total_sectors - rel_lba) return 0;
	return fs->disk.read(fs->disk.ctx, fs->disk.base_lba + rel_lba, dst, sectors);
}

static int disk_write(const cellfs_t *fs, uint32_t rel_lba, const void *src, uint32_t sectors) {
	if (!fs || !fs->disk.write || !src || !sectors) return 0;
	if (rel_lba >= fs->disk.total_sectors || sectors > fs->disk.total_sectors - rel_lba) return 0;
	return fs->disk.write(fs->disk.ctx, fs->disk.base_lba + rel_lba, src, sectors);
}

static int write_super(cellfs_t *fs) {
	fs->super.crc32 = super_crc(&fs->super);
	return disk_write(fs, 0, &fs->super, 1);
}

static int write_inode(cellfs_t *fs, uint32_t idx) {
	if (idx >= CELLFS_MAX_INODES) return 0;
	fs->inodes[idx].crc32 = inode_crc(&fs->inodes[idx]);
	uint8_t sector[CELLFS_SECTOR_SIZE];
	uint32_t per_sector = CELLFS_SECTOR_SIZE / CELLFS_INODE_BYTES;
	uint32_t sector_index = idx / per_sector;
	uint32_t first = sector_index * per_sector;
	for (uint32_t i = 0; i < per_sector; ++i)
		bytes_copy(sector + i * CELLFS_INODE_BYTES, &fs->inodes[first + i], CELLFS_INODE_BYTES);
	return disk_write(fs, fs->super.inode_table_lba + sector_index, sector, 1);
}

static void inode_init(cellfs_inode_t *ino, uint32_t id, uint32_t parent, uint8_t type, const char *name, uint32_t gen) {
	bytes_zero(ino, sizeof(*ino));
	ino->id = id;
	ino->parent = parent;
	ino->type = type;
	ino->generation = gen;
	if (name) {
		size_t n = str_len_cap(name, CELLFS_NAME_MAX);
		for (size_t i = 0; i < n; ++i) ino->name[i] = name[i];
		ino->name[n] = 0;
	}
	ino->crc32 = inode_crc(ino);
}

int cellfs_format(cellfs_t *fs, const cellfs_disk_t *disk) {
	if (!fs || !disk || !disk->read || !disk->write || disk->total_sectors <= CELLFS_DATA_LBA) return 0;
	bytes_zero(fs, sizeof(*fs));
	fs->disk = *disk;
	fs->super.magic = CELLFS_MAGIC;
	fs->super.version = CELLFS_VERSION;
	fs->super.header_bytes = sizeof(cellfs_super_t);
	fs->super.sector_size = CELLFS_SECTOR_SIZE;
	fs->super.total_sectors = disk->total_sectors;
	fs->super.inode_count = CELLFS_MAX_INODES;
	fs->super.inode_entry_bytes = CELLFS_INODE_BYTES;
	fs->super.inode_table_lba = 1u;
	fs->super.inode_table_sectors = CELLFS_INODE_TABLE_SECTORS;
	fs->super.data_lba = CELLFS_DATA_LBA;
	fs->super.next_data_lba = CELLFS_DATA_LBA;
	fs->super.generation = 1u;
	inode_init(&fs->inodes[0], 1u, 1u, CELLFS_TYPE_DIR, "", fs->super.generation);
	inode_init(&fs->inodes[1], 2u, 1u, CELLFS_TYPE_DIR, "home", fs->super.generation);
	inode_init(&fs->inodes[2], 3u, 1u, CELLFS_TYPE_DIR, "programs", fs->super.generation);
	for (uint32_t i = 3u; i < CELLFS_MAX_INODES; ++i) {
		bytes_zero(&fs->inodes[i], sizeof(fs->inodes[i]));
		fs->inodes[i].crc32 = inode_crc(&fs->inodes[i]);
	}
	if (!write_super(fs)) return 0;
	for (uint32_t s = 0; s < CELLFS_INODE_TABLE_SECTORS; ++s) {
		uint8_t sector[CELLFS_SECTOR_SIZE];
		uint32_t first = s * (CELLFS_SECTOR_SIZE / CELLFS_INODE_BYTES);
		for (uint32_t i = 0; i < CELLFS_SECTOR_SIZE / CELLFS_INODE_BYTES; ++i)
			bytes_copy(sector + i * CELLFS_INODE_BYTES, &fs->inodes[first+i], CELLFS_INODE_BYTES);
		if (!disk_write(fs, fs->super.inode_table_lba + s, sector, 1)) return 0;
	}
	fs->mounted = 1;
	return 1;
}

int cellfs_mount(cellfs_t *fs, const cellfs_disk_t *disk) {
	if (!fs || !disk || !disk->read || !disk->write) return 0;
	bytes_zero(fs, sizeof(*fs));
	fs->disk = *disk;
	if (!disk_read(fs, 0, &fs->super, 1)) return 0;
	if (fs->super.magic != CELLFS_MAGIC || fs->super.version != CELLFS_VERSION ||
	    fs->super.header_bytes != sizeof(cellfs_super_t) || fs->super.sector_size != CELLFS_SECTOR_SIZE ||
	    fs->super.total_sectors != disk->total_sectors || fs->super.inode_count != CELLFS_MAX_INODES ||
	    fs->super.inode_entry_bytes != CELLFS_INODE_BYTES || fs->super.inode_table_lba != 1u ||
	    fs->super.inode_table_sectors != CELLFS_INODE_TABLE_SECTORS || fs->super.data_lba != CELLFS_DATA_LBA ||
	    fs->super.next_data_lba < CELLFS_DATA_LBA || fs->super.next_data_lba > disk->total_sectors ||
	    fs->super.crc32 != super_crc(&fs->super)) return 0;
	for (uint32_t s = 0; s < CELLFS_INODE_TABLE_SECTORS; ++s) {
		uint8_t sector[CELLFS_SECTOR_SIZE];
		if (!disk_read(fs, fs->super.inode_table_lba + s, sector, 1)) return 0;
		uint32_t first = s * (CELLFS_SECTOR_SIZE / CELLFS_INODE_BYTES);
		for (uint32_t i = 0; i < CELLFS_SECTOR_SIZE / CELLFS_INODE_BYTES; ++i)
			bytes_copy(&fs->inodes[first+i], sector + i * CELLFS_INODE_BYTES, CELLFS_INODE_BYTES);
	}
	if (!inode_table_valid(fs)) return 0;
	fs->mounted = 1;
	return 1;
}

const cellfs_inode_t *cellfs_inode(const cellfs_t *fs, uint32_t inode_id) {
	if (!fs || !fs->mounted || !inode_id || inode_id > CELLFS_MAX_INODES) return 0;
	const cellfs_inode_t *ino = &fs->inodes[inode_id - 1u];
	return ino->type == CELLFS_TYPE_FREE ? 0 : ino;
}

int cellfs_find_child(const cellfs_t *fs, uint32_t parent, const char *name, uint32_t *inode_id) {
	if (!fs || !fs->mounted || !name || !inode_id) return 0;
	for (uint32_t i = 0; i < CELLFS_MAX_INODES; ++i) {
		const cellfs_inode_t *ino = &fs->inodes[i];
		if (ino->type != CELLFS_TYPE_FREE && ino->parent == parent && str_eq(ino->name, name)) {
			*inode_id = ino->id;
			return 1;
		}
	}
	return 0;
}

static int create_inode(cellfs_t *fs, uint32_t parent, const char *name, uint8_t type, uint32_t *inode_id) {
	if (!fs || !fs->mounted || !name_valid(name) || !cellfs_inode(fs, parent)) return 0;
	if (cellfs_inode(fs, parent)->type != CELLFS_TYPE_DIR) return 0;
	uint32_t found;
	if (cellfs_find_child(fs, parent, name, &found)) return 0;
	for (uint32_t i = 1u; i < CELLFS_MAX_INODES; ++i) {
		if (fs->inodes[i].type == CELLFS_TYPE_FREE) {
			++fs->super.generation;
			inode_init(&fs->inodes[i], i + 1u, parent, type, name, fs->super.generation);
			if (!write_inode(fs, i) || !write_super(fs)) return 0;
			if (inode_id) *inode_id = i + 1u;
			return 1;
		}
	}
	return 0;
}

int cellfs_create_dir(cellfs_t *fs, uint32_t parent, const char *name, uint32_t *inode_id) {
	return create_inode(fs, parent, name, CELLFS_TYPE_DIR, inode_id);
}

int cellfs_create_file(cellfs_t *fs, uint32_t parent, const char *name, uint32_t *inode_id) {
	return create_inode(fs, parent, name, CELLFS_TYPE_FILE, inode_id);
}

int cellfs_read_file(const cellfs_t *fs, uint32_t inode_id, void *dst, size_t cap, size_t *bytes_out) {
	const cellfs_inode_t *ino = cellfs_inode(fs, inode_id);
	if (!ino || ino->type != CELLFS_TYPE_FILE || !dst) return 0;
	if (ino->size > cap) return 0;
	if (!ino->size) { if (bytes_out) *bytes_out = 0; return 1; }
	uint8_t sector[CELLFS_SECTOR_SIZE];
	uint8_t *d = (uint8_t *)dst;
	size_t left = ino->size;
	for (uint32_t s = 0; s < ino->data_sectors; ++s) {
		if (!disk_read(fs, ino->data_lba + s, sector, 1)) return 0;
		size_t n = left > CELLFS_SECTOR_SIZE ? CELLFS_SECTOR_SIZE : left;
		bytes_copy(d, sector, n);
		d += n;
		left -= n;
	}
	if (bytes_out) *bytes_out = ino->size;
	return 1;
}

int cellfs_write_file(cellfs_t *fs, uint32_t inode_id, const void *data, size_t bytes, int append) {
	if (!fs || !fs->mounted || !data || bytes > CELLFS_FILE_MAX) return 0;
	cellfs_inode_t *ino = inode_id && inode_id <= CELLFS_MAX_INODES ? &fs->inodes[inode_id - 1u] : 0;
	if (!ino || ino->type != CELLFS_TYPE_FILE) return 0;
	uint8_t *merged = fs->scratch;
	size_t old_bytes = 0;
	if (append && ino->size) {
		if ((size_t)ino->size + bytes > CELLFS_FILE_MAX) return 0;
		if (!cellfs_read_file(fs, inode_id, merged, CELLFS_FILE_MAX, &old_bytes)) return 0;
	}
	if (old_bytes + bytes > CELLFS_FILE_MAX) return 0;
	bytes_copy(merged + old_bytes, data, bytes);
	size_t total = old_bytes + bytes;
	uint32_t sectors = (uint32_t)((total + CELLFS_SECTOR_SIZE - 1u) / CELLFS_SECTOR_SIZE);
	uint32_t start = fs->super.next_data_lba;
	if (sectors && (start >= fs->disk.total_sectors || sectors > fs->disk.total_sectors - start)) return 0;
	uint8_t sector[CELLFS_SECTOR_SIZE];
	size_t off = 0;
	for (uint32_t s = 0; s < sectors; ++s) {
		bytes_zero(sector, sizeof(sector));
		size_t n = total - off;
		if (n > CELLFS_SECTOR_SIZE) n = CELLFS_SECTOR_SIZE;
		bytes_copy(sector, merged + off, n);
		if (!disk_write(fs, start + s, sector, 1)) return 0;
		off += n;
	}
	++fs->super.generation;
	ino->size = (uint32_t)total;
	ino->data_lba = sectors ? start : 0u;
	ino->data_sectors = sectors;
	ino->generation = fs->super.generation;
	fs->super.next_data_lba = start + sectors;
	if (!write_inode(fs, inode_id - 1u) || !write_super(fs)) return 0;
	return 1;
}

int cellfs_remove(cellfs_t *fs, uint32_t inode_id, int require_dir) {
	if (!fs || !fs->mounted || inode_id <= 3u || inode_id > CELLFS_MAX_INODES) return 0;
	cellfs_inode_t *ino = &fs->inodes[inode_id - 1u];
	if (ino->type == CELLFS_TYPE_FREE) return 0;
	if (require_dir && ino->type != CELLFS_TYPE_DIR) return 0;
	if (!require_dir && ino->type != CELLFS_TYPE_FILE) return 0;
	if (ino->type == CELLFS_TYPE_DIR) {
		for (uint32_t i = 0; i < CELLFS_MAX_INODES; ++i)
			if (fs->inodes[i].type != CELLFS_TYPE_FREE && fs->inodes[i].parent == inode_id) return 0;
	}
	++fs->super.generation;
	bytes_zero(ino, sizeof(*ino));
	ino->crc32 = inode_crc(ino);
	if (!write_inode(fs, inode_id - 1u) || !write_super(fs)) return 0;
	return 1;
}

int cellfs_sync(cellfs_t *fs) {
	if (!fs || !fs->mounted) return 0;
	return write_super(fs);
}
