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
#include "core/syscall.h"

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

static int install(cell_vfs_t *vfs, const char *name, const uint8_t *image, size_t image_bytes) {
	uint32_t programs = 0, id = 0;
	if (!cellfs_find_child(&vfs->fs, 1, "programs", &programs)) return 0;
	if (!cellfs_create_file(&vfs->fs, programs, name, &id)) return 0;
	return cellfs_write_file(&vfs->fs, id, image, image_bytes, 0);
}

int main(void) {
	static uint8_t image[CELL_EXEC_FILE_MAX];
	const char *source =
		"#include <fcntl.h>\n"
		"#include <unistd.h>\n"
		"#include <stdlib.h>\n"
		"#include <errno.h>\n"
		"int main(void) {\n"
		" int fd; int n; char *buf;\n"
		" fd = open(\"/home/native.txt\", O_CREAT | O_TRUNC | O_RDWR);\n"
		" if (fd < 0) return errno;\n"
		" n = write(fd, \"cell io\\n\", 8);\n"
		" if (same(n, 8) == 0) return 40 + errno;\n"
		" if (lseek(fd, 0, SEEK_SET) < 0) return 50 + errno;\n"
		" buf = malloc(16);\n"
		" if (buf == 0) return 60 + errno;\n"
		" n = read(fd, buf, 8);\n"
		" if (n != 8) return 70 + errno;\n"
		" if (write(STDOUT_FILENO, buf, n) != n) return 80 + errno;\n"
		" if (read(STDIN_FILENO, buf, 1) != 0) return 81;\n"
		" if (write(STDERR_FILENO, \"err\\n\", 4) != 4) return 82 + errno;\n"
		" free(buf);\n"
		" if (close(fd) != 0) return 90 + errno;\n"
		" return 0;\n"
		"}\n"
		"int same(int a, int b) { if (a == b) return 1; return 0; }\n";
	size_t image_bytes = 0;
	cell_cc_diag_t diag;
	int ok = 1;
	ok &= expect(cell_cc_compile(source, strlen(source), image, sizeof(image), &image_bytes, &diag),
		"compile C system interface program", diag.message);
	cell_exec_t exec;
	ok &= expect(cell_exec_open(&exec, image, image_bytes, ~0ull) == CELL_EXEC_OK,
		"verify C system interface image", 0);

	memset(disk_bytes, 0, sizeof(disk_bytes));
	cellfs_disk_t disk = {rd, wr, 0, 0, SECTORS};
	cellfs_t formatter;
	cell_vfs_t vfs;
	if (!cellfs_format(&formatter, &disk) || !cell_vfs_mount(&vfs, &disk, 0, 0, 0, 0, 0)) return 1;
	if (!install(&vfs, "sysio", image, image_bytes)) return 1;

	cell_task_manager_t tasks;
	cell_task_manager_init(&tasks, cell_task_default_policy());
	cell_capability_env_t env = {0};
	env.vfs = &vfs; env.tasks = &tasks; env.cortex_ready = 1; env.ata0_ready = 1;
	char out[1024]; uint32_t pid = 0;
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/sysio", out, sizeof(out), &pid),
		"run C system interface program", out);
	ok &= expect(strcmp(out, "cell io\nerr\n") == 0, "stdout/stderr descriptor output", out);
	char ps[256];
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 0") != 0,
		"system interface program exit status", ps);
	char file[32]; size_t got = 0;
	ok &= expect(cell_vfs_read_bytes(&vfs, "/home/native.txt", file, sizeof(file), &got) == CELL_VFS_OK &&
		got == 8 && memcmp(file, "cell io\n", 8) == 0, "persistent file I/O", 0);

	const char *errors =
		"#include <fcntl.h>\n#include <unistd.h>\n#include <errno.h>\n"
		"int main(void) { int fd; fd = open(\"/programs/forbidden\", O_CREAT | O_WRONLY); "
		"if (fd >= 0) return 99; if (errno != EACCES) return 98; "
		"if (close(7) == 0) return 97; return errno; }\n";
	ok &= expect(cell_cc_compile(errors, strlen(errors), image, sizeof(image), &image_bytes, &diag),
		"compile errno and protection program", diag.message);
	if (!install(&vfs, "syserr", image, image_bytes)) return 1;
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/syserr", out, sizeof(out), &pid),
		"run errno and protection program", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 9") != 0,
		"EBADF visible through errno", ps);

	const char *live =
		"#include <fcntl.h>\n#include <unistd.h>\n#include <stdlib.h>\n"
		"int main(void) { int fd; int n; char *p; fd = open(\"/sys/cortex/status\", O_RDONLY); "
		"if (fd < 0) return 91; p = malloc(256); if (p == 0) return 92; "
		"n = read(fd, p, 255); if (n <= 0) return 93; write(STDOUT_FILENO, p, n); close(fd); free(p); return 0; }\n";
	ok &= expect(cell_cc_compile(live, strlen(live), image, sizeof(image), &image_bytes, &diag),
		"compile live VFS descriptor program", diag.message);
	if (!install(&vfs, "syslive", image, image_bytes)) return 1;
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/syslive", out, sizeof(out), &pid),
		"run live VFS descriptor program", out);
	ok &= expect(strncmp(out, "Cell OS is running.", 19) == 0, "live VFS read through fd", out);

	const char *functions =
		"#include <unistd.h>\n#include <stdlib.h>\n"
		"int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }\n"
		"int main(void) { char *p; int v; p = malloc(4); if (p == 0) return 91; "
		"p[0] = 'O'; p[1] = 'K'; p[2] = '\\n'; p[3] = 0; "
		"write(STDOUT_FILENO, p, 3); v = fact(5); free(p); return v; }\n";
	ok &= expect(cell_cc_compile(functions, strlen(functions), image, sizeof(image), &image_bytes, &diag),
		"compile functions and pointer stores", diag.message);
	if (!install(&vfs, "sysfunc", image, image_bytes)) return 1;
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/sysfunc", out, sizeof(out), &pid),
		"run functions and pointer stores", out);
	ok &= expect(strcmp(out, "OK\n") == 0, "pointer-store output", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 120") != 0,
		"bounded recursive user function", ps);

	const char *synthetic =
		"#include <fcntl.h>\n#include <unistd.h>\n#include <stdlib.h>\n#include <errno.h>\n"
		"int dump(char *path) { int fd; int n; char *p; fd = open(path, O_RDONLY); if (fd < 0) return 1; "
		"p = malloc(256); if (p == 0) return 2; n = read(fd, p, 255); if (n <= 0) return 3; "
		"if (write(STDOUT_FILENO, p, n) != n) return 4; free(p); return close(fd); }\n"
		"int main(void) { int fd; int n; char *p; if (dump(\"/proc/self/status\") != 0) return 10; "
		"if (dump(\"/sys/cpu/info\") != 0) return 20; p = malloc(4); if (p == 0) return 30; "
		"fd = open(\"/dev/zero\", O_RDONLY); if (fd < 0) return 31; n = read(fd, p, 4); if (n != 4) return 32; "
		"if (p[0] != 0) return 33; if (p[1] != 0) return 34; if (lseek(fd, 0, SEEK_SET) >= 0) return 35; "
		"if (errno != ESPIPE) return 36; close(fd); fd = open(\"/dev/null\", O_WRONLY); if (fd < 0) return 40; "
		"if (write(fd, \"discard\", 7) != 7) return 41; close(fd); fd = open(\"/dev/console\", O_WRONLY); "
		"if (fd < 0) return 50; if (write(fd, \"console ok\\n\", 11) != 11) return 51; close(fd); free(p); return 0; }\n";

	ok &= expect(cell_cc_compile(synthetic, strlen(synthetic), image, sizeof(image), &image_bytes, &diag),
		"compile synthetic namespace program", diag.message);
	if (!install(&vfs, "sysview", image, image_bytes)) return 1;
	ok &= expect(cell_task_run(&tasks, &vfs, &env, "/programs/sysview", out, sizeof(out), &pid),
		"run synthetic namespace program", out);
	ok &= expect(strstr(out, "running -") != 0 && strstr(out, "/programs/sysview") != 0,
		"/proc/self visible through fd", out);
	ok &= expect(strstr(out, "CPU: ") != 0 && strstr(out, "logical processors") != 0,
		"/sys/cpu/info visible through fd", out);
	ok &= expect(strstr(out, "console ok\n") != 0, "/dev/console write through fd", out);
	ok &= expect(cell_task_describe(&tasks, pid, ps, sizeof(ps)) && strstr(ps, "exited 0") != 0,
		"synthetic namespace program exit status", ps);

	if (!ok) return 1;
	puts("#C-SYSTEM file descriptor table PASS");
	puts("#C-SYSTEM open/close/read/write/lseek PASS");
	puts("#C-SYSTEM stdin/stdout/stderr descriptor ABI PASS");
	puts("#C-SYSTEM errno propagation PASS");
	puts("#C-SYSTEM bounded malloc/free PASS");
	puts("#C-SYSTEM /home write boundary PASS");
	puts("#C-SYSTEM live VFS descriptor reads PASS");
	puts("#C-SYSTEM user-defined functions and bounded call stack PASS");
	puts("#C-SYSTEM pointer-indexed byte stores PASS");
	puts("#SYNTHFS C /proc self access PASS");
	puts("#SYNTHFS C /sys capability projection PASS");
	puts("#SYNTHFS C /dev null zero console semantics PASS");
	return 0;
}
