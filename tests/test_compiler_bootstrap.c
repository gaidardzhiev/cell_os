/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/cc.h"
#include "core/capability.h"
#include "core/cellfs.h"
#include "core/cellexec.h"
#include "core/task.h"
#include "core/vfs.h"

#define SECTORS 1024u

static uint8_t disk_bytes[SECTORS * CELLFS_SECTOR_SIZE];
static uint8_t source[CELL_CC_SOURCE_MAX];
static uint8_t stage1_image[CELL_EXEC_FILE_MAX];
static uint8_t generated_image[CELL_EXEC_FILE_MAX];

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
	if (got) fprintf(stderr, "\n got: %s", got);
	fputc('\n', stderr);
	return 0;
}

static int install(cell_vfs_t *vfs, const char *name,
	const uint8_t *image, size_t image_bytes) {
	uint32_t programs = 0, id = 0;
	if (!cellfs_find_child(&vfs->fs, 1, "programs", &programs)) return 0;
	if (!cellfs_create_file(&vfs->fs, programs, name, &id)) return 0;
	return cellfs_write_file(&vfs->fs, id, image, image_bytes, 0);
}

static int read_source(const char *path, size_t *bytes_out) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	size_t n = fread(source, 1, sizeof(source), f);
	int ok = !ferror(f) && feof(f);
	fclose(f);
	if (!ok) return 0;
	*bytes_out = n;
	return 1;
}

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	int ok = 1;
	size_t source_bytes = 0;
	ok &= expect(read_source(argv[1], &source_bytes), "read user-space compiler source", 0);
	if (!ok) return 1;

	cell_cc_diag_t diag;
	size_t stage1_bytes = 0;
	ok &= expect(cell_cc_compile((const char *)source, source_bytes,
		stage1_image, sizeof(stage1_image), &stage1_bytes, &diag),
		"compile user-space compiler", diag.message);
	if (!ok) return 1;

	cell_exec_t stage1;
	ok &= expect(cell_exec_open(&stage1, stage1_image, stage1_bytes, ~0ull) == CELL_EXEC_OK,
		"verify user-space compiler image", 0);
	int saw_compile = 0;
	int saw_install = 0;
	for (uint32_t i = 0; i < stage1.instruction_count; ++i) {
		if (stage1.code[i].opcode != CELL_EXEC_OP_SYSCALL) continue;
		uint8_t nr = CELL_EXEC_SYSCALL_NR(stage1.code[i].imm);
		if (nr == CELL_EXEC_SYS_COMPILE) saw_compile = 1;
		if (nr == CELL_EXEC_SYS_INSTALL_EXEC) saw_install = 1;
	}
	ok &= expect(!saw_compile, "user-space compiler must not invoke trusted compile syscall", 0);
	ok &= expect(saw_install, "user-space compiler must use verified executable install boundary", 0);
	if (!ok) return 1;

	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {rd, wr, 0, 0, SECTORS};
	cellfs_t formatter;
	cell_vfs_t vfs;
	if (!cellfs_format(&formatter, &disk) ||
	    !cell_vfs_mount(&vfs, &disk, 0, 0, 0, 0, 0)) return 1;
	if (!install(&vfs, "cc.stage1", stage1_image, stage1_bytes)) return 1;

	const char *bootstrap_source =
		"#include <stdio.h>\n"
		"int main(void) { puts(\"compiled in CellExec user space\"); return 0; }\n";
	ok &= expect(cell_vfs_write(&vfs, "/home/bootstrap.c", bootstrap_source, 0) == CELL_VFS_OK,
		"write bootstrap source", 0);

	cell_task_manager_t tasks;
	cell_task_manager_init(&tasks, cell_task_default_policy());
	cell_capability_env_t env = {0};
	env.vfs = &vfs;
	env.tasks = &tasks;
	env.cortex_ready = 1;
	env.ata0_ready = 1;

	char out[1024];
	char ps[256];
	uint32_t pid = 0;
	const char *cc_argv[] = {"cc.stage1", "/home/bootstrap.c", "-o", "/programs/bootstrap"};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage1", 4, cc_argv,
		out, sizeof(out), &pid), "run user-space compiler", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 0") != 0,
		"user-space compiler exit status", ps);

	size_t generated_bytes = 0;
	ok &= expect(cell_vfs_read_bytes(&vfs, "/programs/bootstrap", generated_image,
		sizeof(generated_image), &generated_bytes) == CELL_VFS_OK,
		"read generated executable", 0);
	cell_exec_t generated;
	ok &= expect(cell_exec_open(&generated, generated_image, generated_bytes, ~0ull) == CELL_EXEC_OK,
		"verify generated executable", 0);
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/bootstrap",
		out, sizeof(out), &pid), "run generated executable", out);
	ok &= expect(strcmp(out, "compiled in CellExec user space\n") == 0,
		"generated executable output", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 0") != 0,
		"generated executable exit status", ps);

	const char *delegate_source =
		"#include <errno.h>\n"
		"int main(void) { if (install_exec(\"/home/.cc.stage1.cellx\", \"/programs/forbidden\") == 0) return 99; return errno; }\n";
	size_t delegate_bytes = 0;
	ok &= expect(cell_cc_compile(delegate_source, strlen(delegate_source),
		generated_image, sizeof(generated_image), &delegate_bytes, &diag),
		"compile install-boundary delegation probe", diag.message);
	if (!install(&vfs, "notcompiler", generated_image, delegate_bytes)) return 1;
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/notcompiler",
		out, sizeof(out), &pid), "run install-boundary delegation probe", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 13") != 0,
		"install boundary restricted to compiler process", ps);

	if (!ok) return 1;
	printf("#BOOTSTRAP user-space C codegen PASS image=%zu code=%u\n",
		stage1_bytes, stage1.instruction_count);
	puts("#BOOTSTRAP verified executable install boundary PASS");
	puts("#BOOTSTRAP compiler-service independence PASS");
	return 0;
}
