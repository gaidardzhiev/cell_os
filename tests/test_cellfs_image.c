/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/cellfs.h"

typedef struct {
	uint8_t *data;
	uint32_t sectors;
} image_t;

static int image_read(void *ctx, uint64_t lba, void *dst, uint32_t sectors) {
	image_t *img = (image_t *)ctx;
	if (!img || !dst || lba >= img->sectors || sectors > img->sectors - lba) return 0;
	memcpy(dst, img->data + (size_t)lba * CELLFS_SECTOR_SIZE,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}

static int image_write(void *ctx, uint64_t lba, const void *src, uint32_t sectors) {
	image_t *img = (image_t *)ctx;
	if (!img || !src || lba >= img->sectors || sectors > img->sectors - lba) return 0;
	memcpy(img->data + (size_t)lba * CELLFS_SECTOR_SIZE, src,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	FILE *f = fopen(argv[1], "rb");
	if (!f) return 1;
	if (fseek(f, 0, SEEK_END) != 0) return 1;
	long size = ftell(f);
	if (size <= 0 || (size % CELLFS_SECTOR_SIZE) != 0) return 1;
	rewind(f);
	image_t img = {0};
	img.data = malloc((size_t)size);
	img.sectors = (uint32_t)((size_t)size / CELLFS_SECTOR_SIZE);
	if (!img.data || fread(img.data, 1, (size_t)size, f) != (size_t)size) return 1;
	fclose(f);
	cellfs_disk_t disk = {image_read, image_write, &img, 0, img.sectors};
	cellfs_t *fs = calloc(1, sizeof(*fs));
	if (!fs || !cellfs_mount(fs, &disk)) {
		fprintf(stderr, "CellFS image mount failed\n");
		return 1;
	}
	uint32_t home = 0, programs = 0;
	if (!cellfs_find_child(fs, 1, "home", &home) || home != 2 ||
	    !cellfs_find_child(fs, 1, "programs", &programs) || programs != 3) {
		fprintf(stderr, "CellFS seed namespace mismatch\n");
		return 1;
	}
	printf("#CELLFS Python/C ABI PASS sectors=%u generation=%u\n",
		fs->super.total_sectors, fs->super.generation);
	free(fs);
	free(img.data);
	return 0;
}
