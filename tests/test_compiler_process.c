/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/cc.h"
#include "core/cellexec.h"

static unsigned char source[CELL_CC_SOURCE_MAX];
static unsigned char image[CELL_EXEC_FILE_MAX];
static unsigned char image2[CELL_EXEC_FILE_MAX];

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	FILE *f = fopen(argv[1], "rb");
	if (!f) return 3;
	size_t n = fread(source, 1, sizeof(source), f);
	if (ferror(f) || !feof(f)) { fclose(f); return 4; }
	fclose(f);
	cell_cc_diag_t d;
	size_t image_bytes = 0;
	if (!cell_cc_compile((const char *)source, n, image, sizeof(image), &image_bytes, &d)) {
		fprintf(stderr, "compile failed %u:%u: %s\n", d.line, d.column, d.message);
		return 5;
	}
	size_t image_bytes2 = 0;
	cell_cc_diag_t d2;
	if (!cell_cc_compile((const char *)source, n, image2, sizeof(image2), &image_bytes2, &d2)) return 6;
	if (image_bytes != image_bytes2 || memcmp(image, image2, image_bytes) != 0) return 7;
	cell_exec_t e;
	if (cell_exec_open(&e, image, image_bytes, ~0ull) != CELL_EXEC_OK) return 8;
	int seen = 0;
	for (uint32_t i = 0; i < e.instruction_count; ++i) {
		if (e.code[i].opcode == CELL_EXEC_OP_SYSCALL &&
		    CELL_EXEC_SYSCALL_NR(e.code[i].imm) == CELL_EXEC_SYS_COMPILE) seen = 1;
	}
	if (!seen) return 9;
	printf("#CC compiler process boundary PASS image=%zu code=%u deterministic=yes\n", image_bytes, e.instruction_count);
	return 0;
}
