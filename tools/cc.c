/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/cc.h"
#include "core/cellexec.h"

static void usage(FILE *f) { fprintf(f, "usage: cc source.c [-o output]\n"); }

int main(int argc, char **argv) {
	const char *src_path = 0, *out_path = "a.out";
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "-o")) {
			if (++i >= argc) { usage(stderr); return 2; }
			out_path = argv[i];
		} else if (!src_path) src_path = argv[i];
		else { usage(stderr); return 2; }
	}
	if (!src_path) { usage(stderr); return 2; }
	FILE *f = fopen(src_path, "rb");
	if (!f) { perror(src_path); return 1; }
	char source[CELL_CC_SOURCE_MAX];
	size_t n = fread(source, 1, sizeof(source), f);
	if (ferror(f)) { perror(src_path); fclose(f); return 1; }
	if (!feof(f)) { fprintf(stderr, "%s: source file too large\n", src_path); fclose(f); return 1; }
	fclose(f);
	uint8_t image[CELL_EXEC_FILE_MAX]; size_t image_bytes = 0; cell_cc_diag_t d;
	if (!cell_cc_compile(source, n, image, sizeof(image), &image_bytes, &d)) {
		fprintf(stderr, "%s:%u:%u: error: %s\n", src_path, d.line, d.column,
			d.message[0] ? d.message : cell_cc_status_name(d.status));
		return 1;
	}
	f = fopen(out_path, "wb");
	if (!f) { perror(out_path); return 1; }
	if (fwrite(image, 1, image_bytes, f) != image_bytes || fclose(f)) { perror(out_path); return 1; }
	return 0;
}
