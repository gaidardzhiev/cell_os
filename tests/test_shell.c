/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/capability.h"
#include "core/cellexec.h"
#include "core/cellfs.h"
#include "core/shell.h"
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

static int install_hello(cell_vfs_t *vfs) {
	uint8_t blob[64 + 16 + 6];
	memset(blob, 0, sizeof(blob));
	cell_exec_header_t *h = (cell_exec_header_t *)blob;
	h->magic = CELL_EXEC_MAGIC; h->version = CELL_EXEC_VERSION;
	h->header_bytes = 64; h->instruction_bytes = 8; h->code_bytes = 16;
	h->data_bytes = 6; h->entry_pc = 0; h->gas_limit = 16; h->total_bytes = sizeof(blob);
	cell_exec_insn_t *code = (cell_exec_insn_t *)(blob + 64);
	code[0].opcode = CELL_EXEC_OP_PUTS; code[0].a = 6; code[0].imm = 0;
	code[1].opcode = CELL_EXEC_OP_HALT;
	memcpy(blob + 80, "hello\n", 6);
	h->payload_crc32 = cell_exec_crc32(blob + 64, sizeof(blob) - 64);
	uint32_t programs = 0, id = 0;
	if (!cellfs_find_child(&vfs->fs, 1, "programs", &programs)) return 0;
	if (!cellfs_create_file(&vfs->fs, programs, "hello", &id)) return 0;
	return cellfs_write_file(&vfs->fs, id, blob, sizeof(blob), 0);
}

int main(void) {
	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {rd, wr, 0, 0, SECTORS};
	cellfs_t formatter;
	cell_vfs_t vfs;
	if (!cellfs_format(&formatter, &disk) || !cell_vfs_mount(&vfs, &disk, 0, 0, 0, 0, 0)) return 1;
	if (!install_hello(&vfs)) return 1;
	cell_task_manager_t tasks;
	cell_task_manager_init(&tasks, cell_task_default_policy());
	cell_capability_env_t env = {0};
	env.vfs = &vfs; env.tasks = &tasks; env.cortex_ready = 1; env.ata0_ready = 1;
	char out[1024]; uint32_t pid = 0; int ok = 1;
	cell_shell_result_t k;

	k = cell_shell_execute("pwd", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && strcmp(out, "/") == 0, "pwd", out);
	k = cell_shell_execute("cd /home", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "cd is silent", out);
	k = cell_shell_execute("mkdir notes", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "mkdir is silent", out);
	k = cell_shell_execute("echo \"hello world\" > notes/a.txt", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "echo redirection", out);
	k = cell_shell_execute("echo second >> notes/a.txt", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "echo append", out);
	k = cell_shell_execute("cat notes/a.txt", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && strcmp(out, "hello world\nsecond\n") == 0, "cat redirected text", out);
	k = cell_shell_execute("echo 'a b' c", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && strcmp(out, "a b c") == 0, "quoted echo", out);
	k = cell_shell_execute("echo """, &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "empty quoted argument", out);

	k = cell_shell_execute("hello", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && pid == 1 && strcmp(out, "hello\n") == 0, "PATH execution", out);
	k = cell_shell_execute("/programs/hello", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && pid == 2 && strcmp(out, "hello\n") == 0, "absolute execution", out);
	k = cell_shell_execute("hello > /home/hello.out", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && pid == 3 && out[0] == 0, "program stdout redirection", out);
	k = cell_shell_execute("cat /home/hello.out", &env, out, sizeof(out), &pid);
	ok &= expect(strcmp(out, "hello\n") == 0, "program redirected output", out);
	k = cell_shell_execute("ps", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && strstr(out, "PID STATE EXIT GAS COMMAND") == out &&
		strstr(out, "1 exited 0 2 /programs/hello") != 0, "ps", out);
	k = cell_shell_execute("ps -p 2", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && strstr(out, "2 exited 0 2 /programs/hello") != 0, "ps -p", out);

	k = cell_shell_execute("echo '#include <stdio.h>' > /home/hello.c", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "create C source include", out);
	k = cell_shell_execute("echo 'int main(void) { int n = 2; while (n) { puts(\"compiled C\"); n = n - 1; } return 0; }' >> /home/hello.c", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "create C source body", out);
	k = cell_shell_execute("cc /home/hello.c -o /programs/chello", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "cc target compile", out);
	k = cell_shell_execute("chello", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && strcmp(out, "compiled C\ncompiled C\n") == 0, "execute compiled C", out);

	k = cell_shell_execute("echo '#include <stdio.h>' > /home/args.c", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "create argv C source include", out);
	k = cell_shell_execute("echo '#include <stdlib.h>' >> /home/args.c", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "create argv C source stdlib", out);
	k = cell_shell_execute("echo 'int main(int argc, char **argv) { if (argc != 3) return 2; puts(argv[1]); return atoi(argv[2]); }' >> /home/args.c", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "create argv C source body", out);
	k = cell_shell_execute("cc /home/args.c -o /programs/cargs", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && out[0] == 0, "cc argv target compile", out);
	k = cell_shell_execute("cargs \"hello world\" 7", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && strcmp(out, "hello world\n") == 0, "program argv execution", out);
	{
		char want[64];
		(void)snprintf(want, sizeof(want), "%u exited 7", pid);
		k = cell_shell_execute("ps", &env, out, sizeof(out), &pid);
		ok &= expect(k == CELL_SHELL_PROCESS && strstr(out, want) != 0, "argv program exit status", out);
	}

	cell_vfs_t remount;
	cell_task_manager_t tasks2;
	cell_capability_env_t env2 = {0};
	if (!cell_vfs_mount(&remount, &disk, 0, 0, 0, 0, 0)) return 1;
	cell_task_manager_init(&tasks2, cell_task_default_policy());
	env2.vfs = &remount; env2.tasks = &tasks2; env2.cortex_ready = 1; env2.ata0_ready = 1;
	k = cell_shell_execute("/programs/chello", &env2, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_PROCESS && strcmp(out, "compiled C\ncompiled C\n") == 0, "compiled C persists across remount", out);

	ok &= expect(cell_shell_execute("run hello", &env, out, sizeof(out), &pid) == CELL_SHELL_NOT_HANDLED,
		"legacy run removed", out);
	ok &= expect(cell_shell_execute("tasks", &env, out, sizeof(out), &pid) == CELL_SHELL_NOT_HANDLED,
		"legacy tasks removed", out);
	ok &= expect(cell_shell_execute("write a b", &env, out, sizeof(out), &pid) == CELL_SHELL_NOT_HANDLED,
		"legacy write removed", out);
	ok &= expect(cell_shell_execute("show memory", &env, out, sizeof(out), &pid) == CELL_SHELL_NOT_HANDLED,
		"natural language falls through", out);
	k = cell_shell_execute("echo a | cat", &env, out, sizeof(out), &pid);
	ok &= expect(k == CELL_SHELL_VFS && strstr(out, "not implemented") != 0, "future pipe boundary", out);

	if (!ok) return 1;
	puts("#SHELL POSIX command names PASS");
	puts("#SHELL quoting/redirection PASS");
	puts("#SHELL /programs PATH execution PASS");
	puts("#SHELL legacy proof commands absent PASS");
	puts("#SHELL cc compile/install/run PASS");
	puts("#SHELL compiled-program remount persistence PASS");
	puts("#SHELL program argv forwarding PASS");
	return 0;
}
