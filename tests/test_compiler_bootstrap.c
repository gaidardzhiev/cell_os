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
#include "core/syscall.h"
#include "core/vfs.h"

#define SECTORS 1024u

static uint8_t disk_bytes[SECTORS * CELLFS_SECTOR_SIZE];
static uint8_t source[CELL_CC_SOURCE_MAX + 1u];
static uint8_t image_a[CELL_EXEC_FILE_MAX];
static uint8_t image_b[CELL_EXEC_FILE_MAX];

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
	if (got && *got) fprintf(stderr, "\n got: %s", got);
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
	size_t n = fread(source, 1, CELL_CC_SOURCE_MAX, f);
	int ok = !ferror(f) && feof(f) && n < sizeof(source);
	fclose(f);
	if (!ok) return 0;
	source[n] = 0;
	*bytes_out = n;
	return 1;
}

static int task_exited(cell_task_manager_t *tasks, uint32_t pid,
	int status, char *desc, size_t desc_cap) {
	char needle[32];
	if (!cell_task_describe(tasks, pid, desc, desc_cap)) return 0;
	snprintf(needle, sizeof(needle), "exited %d", status);
	return strstr(desc, needle) != 0;
}

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	int ok = 1;
	size_t source_bytes = 0;
	ok &= expect(read_source(argv[1], &source_bytes), "read compiler source", 0);
	if (!ok) return 1;

	cell_cc_diag_t diag;
	size_t stage1_bytes = 0;
	ok &= expect(cell_cc_compile((const char *)source, source_bytes,
		image_a, sizeof(image_a), &stage1_bytes, &diag),
		"build stage1 with bootstrap compiler", diag.message);
	if (!ok) return 1;

	cell_exec_t stage1;
	ok &= expect(cell_exec_open(&stage1, image_a, stage1_bytes, ~0ull) == CELL_EXEC_OK,
		"verify stage1", 0);
	int saw_compile = 0;
	int saw_install = 0;
	for (uint32_t i = 0; i < stage1.instruction_count; ++i) {
		if (stage1.code[i].opcode != CELL_EXEC_OP_SYSCALL) continue;
		uint8_t nr = CELL_EXEC_SYSCALL_NR(stage1.code[i].imm);
		if (nr == CELL_EXEC_SYS_COMPILE) saw_compile = 1;
		if (nr == CELL_EXEC_SYS_INSTALL_EXEC) saw_install = 1;
	}
	ok &= expect(!saw_compile, "stage1 must not invoke trusted compile syscall", 0);
	ok &= expect(saw_install, "stage1 must use verified executable install boundary", 0);
	if (!ok) return 1;

	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {rd, wr, 0, 0, SECTORS};
	cellfs_t formatter;
	cell_vfs_t vfs;
	if (!cellfs_format(&formatter, &disk) ||
	    !cell_vfs_mount(&vfs, &disk, 0, 0, 0, 0, 0)) return 1;
	if (!install(&vfs, "cc.stage1", image_a, stage1_bytes)) return 1;
	ok &= expect(cell_vfs_write(&vfs, "/home/cc.stage1.c", (const char *)source, 0) == CELL_VFS_OK,
		"write compiler source into CellFS", 0);
	if (!ok) return 1;

	cell_task_manager_t tasks;
	cell_task_manager_init(&tasks, cell_task_default_policy());
	cell_capability_env_t env = {0};
	env.vfs = &vfs;
	env.tasks = &tasks;
	env.cortex_ready = 1;
	env.ata0_ready = 1;

	char out[1024];
	char desc[256];
	uint32_t pid = 0;
	const char *stage1_argv[] = {
		"cc.stage1", "/home/cc.stage1.c", "-o", "/programs/cc.stage2"
	};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage1",
		4, stage1_argv, out, sizeof(out), &pid), "stage1 builds stage2", out);
	ok &= expect(task_exited(&tasks, pid, 0, desc, sizeof(desc)),
		"stage1 exit status", desc);
	if (!ok) return 1;

	size_t stage2_bytes = 0;
	ok &= expect(cell_vfs_read_bytes(&vfs, "/programs/cc.stage2", image_a,
		sizeof(image_a), &stage2_bytes) == CELL_VFS_OK, "read installed stage2", 0);
	cell_exec_t stage2;
	ok &= expect(cell_exec_open(&stage2, image_a, stage2_bytes, ~0ull) == CELL_EXEC_OK,
		"verify stage2", 0);
	if (!ok) return 1;

	const char *stage2_argv[] = {
		"cc.stage2", "--emit", "/home/cc.stage1.c", "-o", "/home/cc.stage3.cellx"
	};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage2",
		5, stage2_argv, out, sizeof(out), &pid), "stage2 emits stage3 candidate", out);
	ok &= expect(task_exited(&tasks, pid, 0, desc, sizeof(desc)),
		"stage2 self-compile exit status", desc);
	if (!ok) return 1;

	const char *install3_argv[] = {
		"cc.stage1", "--install", "/home/cc.stage3.cellx", "/programs/cc.stage3"
	};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage1",
		4, install3_argv, out, sizeof(out), &pid), "install verified stage3", out);
	ok &= expect(task_exited(&tasks, pid, 0, desc, sizeof(desc)),
		"stage3 install exit status", desc);
	if (!ok) return 1;

	size_t stage3_bytes = 0;
	ok &= expect(cell_vfs_read_bytes(&vfs, "/programs/cc.stage3", image_b,
		sizeof(image_b), &stage3_bytes) == CELL_VFS_OK, "read installed stage3", 0);
	cell_exec_t stage3;
	ok &= expect(cell_exec_open(&stage3, image_b, stage3_bytes, ~0ull) == CELL_EXEC_OK,
		"verify stage3", 0);
	ok &= expect(stage2_bytes == stage3_bytes &&
		memcmp(image_a, image_b, stage2_bytes) == 0,
		"stage2/stage3 bootstrap fixed point", 0);
	if (!ok) return 1;

	const char *denied_argv[] = {
		"cc.stage2", "--install", "/home/cc.stage3.cellx", "/programs/forbidden"
	};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage2",
		4, denied_argv, out, sizeof(out), &pid), "run stage2 install-denial probe", out);
	ok &= expect(task_exited(&tasks, pid, -1, desc, sizeof(desc)),
		"self-hosted stage must not inherit install privilege", desc);
	ok &= expect(tasks.errno_value == CELL_EACCES,
		"denied install must report EACCES", 0);
	size_t forbidden_bytes = 0;
	ok &= expect(cell_vfs_read_bytes(&vfs, "/programs/forbidden", image_b,
		sizeof(image_b), &forbidden_bytes) == CELL_VFS_NOT_FOUND,
		"denied install must not create executable", 0);

	const char *proof_source =
		"#include <stdio.h>\n"
		"int main(void) { puts(\"compiled by self-hosted cc\"); return 0; }\n";
	ok &= expect(cell_vfs_write(&vfs, "/home/selfhost.c", proof_source, 0) == CELL_VFS_OK,
		"write self-host proof source", 0);
	const char *proof_emit_argv[] = {
		"cc.stage2", "--emit", "/home/selfhost.c", "-o", "/home/selfhost.cellx"
	};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage2",
		5, proof_emit_argv, out, sizeof(out), &pid), "self-hosted compiler emits proof program", out);
	ok &= expect(task_exited(&tasks, pid, 0, desc, sizeof(desc)),
		"self-hosted proof compile exit status", desc);
	const char *proof_install_argv[] = {
		"cc.stage1", "--install", "/home/selfhost.cellx", "/programs/selfhost-proof"
	};
	ok &= expect(cell_task_run_argv(&tasks, &vfs, &env, "/programs/cc.stage1",
		4, proof_install_argv, out, sizeof(out), &pid), "install self-hosted proof program", out);
	ok &= expect(task_exited(&tasks, pid, 0, desc, sizeof(desc)),
		"self-hosted proof install exit status", desc);
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/selfhost-proof",
		out, sizeof(out), &pid), "run self-hosted proof program", out);
	ok &= expect(strcmp(out, "compiled by self-hosted cc\n") == 0,
		"self-hosted proof output", out);
	ok &= expect(task_exited(&tasks, pid, 0, desc, sizeof(desc)),
		"self-hosted proof execution status", desc);

	if (!ok) return 1;
	printf("#BOOTSTRAP stage1->stage2 PASS stage1=%zu stage2=%zu\n",
		stage1_bytes, stage2_bytes);
	printf("#BOOTSTRAP stage2->stage3 fixed point PASS bytes=%zu code=%u\n",
		stage3_bytes, stage3.instruction_count);
	puts("#BOOTSTRAP self-hosted compiler codegen PASS");
	puts("#BOOTSTRAP verified executable install boundary PASS");
	return 0;
}
