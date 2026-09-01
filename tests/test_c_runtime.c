/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/cc.h"
#include "core/capability.h"
#include "core/cellfs.h"
#include "core/task.h"
#include "core/vfs.h"

#define SECTORS 1024u
static uint8_t disk_bytes[SECTORS * CELLFS_SECTOR_SIZE];

static int rd(void *ctx, uint64_t lba, void *dst, uint32_t sectors) {
	(void)ctx;
	if (!dst || lba >= SECTORS || sectors > SECTORS - lba) return 0;
	memcpy(dst, disk_bytes + (size_t)lba * CELLFS_SECTOR_SIZE,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}
static int wr(void *ctx, uint64_t lba, const void *src, uint32_t sectors) {
	(void)ctx;
	if (!src || lba >= SECTORS || sectors > SECTORS - lba) return 0;
	memcpy(disk_bytes + (size_t)lba * CELLFS_SECTOR_SIZE, src,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}
static int expect(int cond, const char *what, const char *got) {
	if (cond) return 1;
	fprintf(stderr, "FAIL: %s", what);
	if (got) fprintf(stderr, "\n  got: %s", got);
	fputc('\n', stderr);
	return 0;
}

int main(void) {
	static uint8_t image[CELL_EXEC_FILE_MAX];
	const char *source =
		"#include <stdio.h>\n"
		"#include <string.h>\n"
		"#include <stdlib.h>\n"
		"int main(int argc, char **argv) {\n"
		" char *name; int n;\n"
		" if (argc != 3) { puts(\"bad argc\"); return 90; }\n"
		" name = argv[1]; puts(name);\n"
		" if (strcmp(name, \"cell\") == 0) putchar('C');\n"
		" putchar('\\n');\n"
		" n = atoi(argv[2]);\n"
		" if (strlen(name) >= 4) n = n + 8 / 2;\n"
		" return n % 100;\n"
		"}\n";
	size_t image_bytes = 0;
	cell_cc_diag_t diag;
	int ok = 1;
	ok &= expect(cell_cc_compile(source, strlen(source), image, sizeof(image), &image_bytes, &diag),
		"compile process C program", diag.message);

	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {rd, wr, 0, 0, SECTORS};
	cellfs_t formatter;
	cell_vfs_t vfs;
	if (!cellfs_format(&formatter, &disk) || !cell_vfs_mount(&vfs, &disk, 0, 0, 0, 0, 0)) return 1;
	uint32_t programs = 0, id = 0;
	if (!cellfs_find_child(&vfs.fs, 1, "programs", &programs) ||
	    !cellfs_create_file(&vfs.fs, programs, "rt", &id) ||
	    !cellfs_write_file(&vfs.fs, id, image, image_bytes, 0)) return 1;

	cell_task_manager_t tasks;
	cell_task_manager_init(&tasks, cell_task_default_policy());
	cell_capability_env_t env = {0};
	env.vfs = &vfs; env.tasks = &tasks; env.cortex_ready = 1; env.ata0_ready = 1;
	char out[1024]; uint32_t pid = 0;
	const char *argv1[] = {"rt", "cell", "42"};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/rt", 3, argv1,
		out, sizeof(out), &pid), "run with argv", out);
	ok &= expect(strcmp(out, "cell\nC\n") == 0, "dynamic string and putchar output", out);
	char ps[256];
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 46") != 0,
		"expression exit status", ps);

	const char *argv2[] = {"rt"};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/rt", 1, argv2,
		out, sizeof(out), &pid), "run argc failure path", out);
	ok &= expect(strcmp(out, "bad argc\n") == 0, "argc visible to program", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 90") != 0,
		"argc failure exit status", ps);

	if (!ok) return 1;
	puts("#C-RUNTIME argc/argv process ABI PASS");
	puts("#C-RUNTIME char pointer and argv indexing PASS");
	puts("#C-RUNTIME puts/putchar/strlen/strcmp/atoi PASS");
	puts("#C-RUNTIME arithmetic comparison and expression return PASS");
	return 0;
}
