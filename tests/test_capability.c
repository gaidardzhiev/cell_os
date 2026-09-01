/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/capability.h"

static int expect(int cond, const char *msg) {
	if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); return 0; }
	return 1;
}

int main(void) {
	int ok = 1;
	cell_capability_id_t id = CELL_CAP_INVALID;
	ok &= expect(cell_capability_parse_call("<CALL memory.status>", 20, &id) && id == CELL_CAP_MEMORY_STATUS, "parse memory.status");
	ok &= expect(cell_capability_parse_call("<CALL cpu.info>\n", 16, &id) && id == CELL_CAP_CPU_INFO, "parse cpu.info with newline");
	ok &= expect(!cell_capability_parse_call("<CALL memory.status extra>", 26, &id), "reject arguments");
	ok &= expect(!cell_capability_parse_call("memory.status", 13, &id), "reject bare name");
	ok &= expect(!cell_capability_parse_call("<CALL evil.mmio>", 16, &id), "reject unknown capability");

	cell_e820_entry_t map[3] = {
		{ .base=0, .length=640u*1024u, .type=1, .attrs=0 },
		{ .base=0x100000u, .length=511ull*1024u*1024u, .type=1, .attrs=0 },
		{ .base=0x20000000u, .length=16u*1024u*1024u, .type=2, .attrs=0 }
	};
	cell_boot_ext_t ext = {0};
	ext.e820_ptr = (uint64_t)(uintptr_t)map;
	ext.e820_count = 3;
	ext.e820_entry_size = sizeof(map[0]);
	handoff_t ho = {0};
	ho.mem_top = 512ull*1024u*1024u;
	cell_mem_arena_t arena = { .next=16ull*1024u*1024u, .end=512ull*1024u*1024u };
	cell_capability_env_t env = { .handoff=&ho, .boot_ext=&ext, .arena=&arena, .cortex_ready=1, .ata0_ready=1 };
	cell_capability_result_t r;
	char human[CELL_CAP_RESPONSE_MAX];

	ok &= expect(cell_capability_execute(&env, CELL_CAP_MEMORY_STATUS, &r), "execute memory");
	ok &= expect(strcmp(r.text, "<RESULT memory.status status=ok top_mib=512 usable_mib=511 free_mib=496>") == 0, "memory result exact");
	ok &= expect(cell_capability_human_response(&r, human, sizeof(human)), "render memory");
	ok &= expect(strcmp(human, "Memory: 511 MiB usable; 496 MiB remain available to Cell OS.") == 0, "memory human exact");

	ok &= expect(cell_capability_execute(&env, CELL_CAP_SYSTEM_STATUS, &r), "execute system");
	ok &= expect(strcmp(r.text, "<RESULT system.status status=ok cortex=ready memory_mib=511 storage=ata0>") == 0, "system result exact");
	ok &= expect(cell_capability_human_response(&r, human, sizeof(human)), "render system");
	ok &= expect(strcmp(human, "Cell OS is running. Cortex is ready; 511 MiB usable memory; storage ata0.") == 0, "system human exact");

	ok &= expect(cell_capability_execute(&env, CELL_CAP_STORAGE_LIST, &r), "execute storage");
	ok &= expect(strcmp(r.text, "<RESULT storage.list status=ok count=1 primary=ata0>") == 0, "storage result exact");
	ok &= expect(cell_capability_human_response(&r, human, sizeof(human)), "render storage");
	ok &= expect(strcmp(human, "Storage: 1 verified device is available as ata0.") == 0, "storage human exact");

	ok &= expect(cell_capability_execute(&env, CELL_CAP_STORAGE_LIST_DIR, &r), "execute unsupported vfs");
	ok &= expect(strcmp(r.text, "<RESULT storage.list_dir status=unsupported reason=no_vfs>") == 0, "unsupported vfs exact");
	ok &= expect(cell_capability_human_response(&r, human, sizeof(human)), "render unsupported vfs");
	ok &= expect(strcmp(human, "Directory listing is unavailable because Cell VFS is not mounted.") == 0, "unsupported human exact");

	ok &= expect(cell_capability_execute(&env, CELL_CAP_CPU_INFO, &r), "execute cpu");
	ok &= expect(strstr(r.text, "<RESULT cpu.info status=ok vendor=") != 0, "cpu result schema");
	ok &= expect(cell_capability_human_response(&r, human, sizeof(human)), "render cpu");
	ok &= expect(strstr(human, "CPU: ") == human, "cpu human prefix");

	if (!ok) return 1;
	puts("#CAPABILITY dispatcher PASS");
	puts("#CAPABILITY strict call parser PASS");
	puts("#CAPABILITY deterministic response PASS");
	return 0;
}
