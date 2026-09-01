/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "cortex/cwm.h"
#include "cortex/cortex.h"
#include "cortex/session.h"
#include "core/cellfs.h"
#include "core/vfs.h"
#include "core/cellexec.h"
#include "core/task.h"

#define FS_SECTORS 1024u
static uint8_t fs_disk[FS_SECTORS * CELLFS_SECTOR_SIZE];

static int fs_read(void *ctx, uint64_t lba, void *dst, uint32_t sectors) {
	(void)ctx;
	if (!dst || lba >= FS_SECTORS || sectors > FS_SECTORS - lba) return 0;
	memcpy(dst, fs_disk + (size_t)lba * CELLFS_SECTOR_SIZE,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}
static int fs_write(void *ctx, uint64_t lba, const void *src, uint32_t sectors) {
	(void)ctx;
	if (!src || lba >= FS_SECTORS || sectors > FS_SECTORS - lba) return 0;
	memcpy(fs_disk + (size_t)lba * CELLFS_SECTOR_SIZE, src,
		(size_t)sectors * CELLFS_SECTOR_SIZE);
	return 1;
}

static int load_file(const char *path, uint8_t **buf, size_t *size) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
	long n = ftell(f);
	if (n <= 0) { fclose(f); return 0; }
	rewind(f);
	*buf = (uint8_t *)malloc((size_t)n);
	if (!*buf) { fclose(f); return 0; }
	if (fread(*buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(*buf); return 0; }
	fclose(f);
	*size = (size_t)n;
	return 1;
}

static int expect(int cond, const char *label, const char *got) {
	if (cond) return 1;
	fprintf(stderr, "FAIL: %s", label);
	if (got) fprintf(stderr, "\n  got: %s", got);
	fputc('\n', stderr);
	return 0;
}

static int install_session_program(cell_vfs_t *vfs) {
	uint8_t blob[64 + 16 + 11];
	memset(blob, 0, sizeof(blob));
	cell_exec_header_t *h = (cell_exec_header_t *)blob;
	h->magic = CELL_EXEC_MAGIC; h->version = CELL_EXEC_VERSION; h->header_bytes = 64;
	h->instruction_bytes = 8; h->code_bytes = 16; h->data_bytes = 11; h->entry_pc = 0;
	h->memory_bytes = 0; h->gas_limit = 16; h->total_bytes = sizeof(blob);
	cell_exec_insn_t *code = (cell_exec_insn_t *)(blob + 64);
	code[0].opcode = CELL_EXEC_OP_PUTS; code[0].a = 11; code[0].imm = 0;
	code[1].opcode = CELL_EXEC_OP_HALT; code[1].imm = 0;
	memcpy(blob + 80, "hello exec\n", 11);
	h->payload_crc32 = cell_exec_crc32(blob + 64, sizeof(blob) - 64);
	uint32_t programs = 0, id = 0;
	if (!cellfs_find_child(&vfs->fs, 1, "programs", &programs)) return 0;
	if (!cellfs_create_file(&vfs->fs, programs, "hello", &id)) return 0;
	return cellfs_write_file(&vfs->fs, id, blob, sizeof(blob), 0);
}

int main(int argc, char **argv) {
	if (argc != 2) { fprintf(stderr, "usage: %s cortex.cwm\n", argv[0]); return 2; }
	uint8_t *blob = 0;
	size_t bytes = 0;
	if (!load_file(argv[1], &blob, &bytes)) { perror("model"); return 1; }
	cwm_model_t m;
	if (!cwm_open(&m, blob, bytes)) { fprintf(stderr, "bad model\n"); free(blob); return 1; }
	size_t wn = cortex_workspace_bytes(&m);
	void *ws = calloc(1, wn);
	cortex_t c;
	if (!ws || !cortex_init(&c, &m, ws, wn)) { fprintf(stderr, "init failed\n"); free(ws); free(blob); return 1; }

	memset(fs_disk, 0, sizeof(fs_disk));
	cellfs_disk_t disk = {fs_read, fs_write, 0, 0, FS_SECTORS};
	cellfs_t *formatter = calloc(1, sizeof(*formatter));
	cell_vfs_t *vfs = calloc(1, sizeof(*vfs));
	if (!formatter || !vfs || !cellfs_format(formatter, &disk) ||
	    !cell_vfs_mount(vfs, &disk, bytes, m.h->vocab_size, m.h->context_len,
	        m.h->d_model, m.h->n_layers)) {
		fprintf(stderr, "vfs init failed\n");
		return 1;
	}
	free(formatter);

	cell_e820_entry_t map[2] = {
		{0, 640u*1024u, 1, 0},
		{0x100000u, 511ull*1024u*1024u, 1, 0}
	};
	cell_boot_ext_t ext = {0};
	ext.e820_ptr = (uint64_t)(uintptr_t)map;
	ext.e820_count = 2;
	ext.e820_entry_size = sizeof(map[0]);
	handoff_t ho = {0};
	ho.mem_top = 512ull*1024u*1024u;
	cell_mem_arena_t arena = {16ull*1024u*1024u, 512ull*1024u*1024u};
	cell_task_manager_t tasks;
	cell_task_manager_init(&tasks, cell_task_default_policy());
	if (!install_session_program(vfs)) { fprintf(stderr, "program install failed\n"); return 1; }
	cell_capability_env_t env = {
		.handoff = &ho, .boot_ext = &ext, .arena = &arena,
		.cortex_ready = 1, .ata0_ready = 1, .vfs = vfs, .tasks = &tasks
	};
	cortex_session_result_t r;
	int ok = 1;

	if (!cortex_session_process(&c, "pwd", &env, &r)) ok = 0;
	else {
		ok &= expect(r.kind == CORTEX_SESSION_VFS, "pwd deterministic path", r.response);
		ok &= expect(strcmp(r.response, "/") == 0, "pwd root", r.response);
	}
	if (!cortex_session_process(&c, "ls", &env, &r)) ok = 0;
	else ok &= expect(strcmp(r.response, "home/  programs/  models/  system/  devices/") == 0,
		"root listing", r.response);
	if (!cortex_session_process(&c, "cd /home", &env, &r)) ok = 0;
	else ok &= expect(r.response[0] == 0, "cd is silent", r.response);
	if (!cortex_session_process(&c, "mkdir notes", &env, &r)) ok = 0;
	else ok &= expect(r.response[0] == 0, "mkdir is silent", r.response);
	if (!cortex_session_process(&c, "echo \"Cell OS remembers this.\" > notes/hello.txt", &env, &r)) ok = 0;
	else ok &= expect(r.response[0] == 0, "echo write", r.response);
	if (!cortex_session_process(&c, "echo Persistent. >> notes/hello.txt", &env, &r)) ok = 0;
	else ok &= expect(r.response[0] == 0, "echo append", r.response);
	if (!cortex_session_process(&c, "cat notes/hello.txt", &env, &r)) ok = 0;
	else ok &= expect(strcmp(r.response, "Cell OS remembers this.\nPersistent.\n") == 0, "cat exact", r.response);
	if (!cortex_session_process(&c, "stat notes/hello.txt", &env, &r)) ok = 0;
	else ok &= expect(strstr(r.response, "file /home/notes/hello.txt size=36") == r.response,
		"stat file", r.response);
	if (!cortex_session_process(&c, "cd /devices", &env, &r)) ok = 0;
	else ok &= expect(r.response[0] == 0, "cd virtual silent", r.response);
	if (!cortex_session_process(&c, "cat cpu", &env, &r)) ok = 0;
	else ok &= expect(strncmp(r.response, "CPU: ", 5) == 0, "cat live cpu", r.response);
	if (!cortex_session_process(&c, "echo no > /system/status", &env, &r)) ok = 0;
	else ok &= expect(strcmp(r.response, "Read-only file system.") == 0, "virtual write refused", r.response);

	if (!cortex_session_process(&c, "hello", &env, &r)) ok = 0;
	else {
		ok &= expect(r.kind == CORTEX_SESSION_TASK, "PATH execution", r.response);
		ok &= expect(strcmp(r.response, "hello exec\n") == 0, "program output", r.response);
	}
	if (!cortex_session_process(&c, "ps", &env, &r)) ok = 0;
	else ok &= expect(strstr(r.response, "PID STATE EXIT GAS COMMAND") == r.response &&
		strstr(r.response, "1 exited 0 2 /programs/hello") != 0, "ps listing", r.response);
	if (!cortex_session_process(&c, "ps -p 1", &env, &r)) ok = 0;
	else ok &= expect(strstr(r.response, "1 exited 0 2 /programs/hello") != 0, "ps -p", r.response);
	if (!cortex_session_process(&c, "touch /home/not-program", &env, &r)) ok = 0;
	else ok &= expect(r.response[0] == 0, "touch home executable candidate", r.response);
	if (!cortex_session_process(&c, "/home/not-program", &env, &r)) ok = 0;
	else ok &= expect(strstr(r.response, "Permission denied") != 0, "execution namespace boundary", r.response);

	/* Natural-language capability routing is validated only with the trained CellLM-1M shape. */
	int capability_model = (m.h->context_len == 256u && m.h->d_model == 128u && m.h->n_layers == 5u);
	if (capability_model) {
		if (!cortex_session_process(&c, "show memory", &env, &r)) ok = 0;
		else {
			ok &= expect(r.kind == CORTEX_SESSION_CAPABILITY, "show memory dispatch", r.first_pass);
			ok &= expect(strcmp(r.first_pass, "<CALL memory.status>") == 0, "memory call", r.first_pass);
		}
		if (!cortex_session_process(&c, "list directories", &env, &r)) ok = 0;
		else {
			ok &= expect(strcmp(r.first_pass, "<CALL storage.list_dir>") == 0,
				"natural list maps to VFS", r.first_pass);
			ok &= expect(strstr(r.response, "cpu") != 0, "natural list uses current /devices", r.response);
		}
	} else {
		puts("#CORTEX natural-language capability session SKIP (demo/non-CellLM model)");
	}

	free(vfs);
	free(ws);
	free(blob);
	if (!ok) return 1;
	puts("#CORTEX VFS command path PASS");
	puts("#CORTEX shared VFS/capability state PASS");
	puts("#CORTEX persistent file operations PASS");
	puts("#CORTEX session routing PASS");
	puts("#CORTEX POSIX shell/process path PASS");
	return 0;
}
