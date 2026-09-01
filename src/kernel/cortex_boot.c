/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stddef.h>
#include <stdint.h>
#include "boot/boot_ext.h"
#include "core/capability.h"
#include "core/handoff.h"
#include "core/mem_arena.h"
#include "core/task.h"
#include "core/vfs.h"
#include "cortex/cwm.h"
#include "cortex/cortex.h"
#include "cortex/session.h"
#include "drivers/ata_pio.h"
#include "kernel/console.h"

#define COM1 0x3F8u
#define CORTEX_MAX_LINE 256u

static inline uint8_t inb(uint16_t port) {
	uint8_t v;
	__asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port));
	return v;
}

static char serial_getc(void) {
	while (!(inb(COM1 + 5u) & 1u)) __asm__ volatile("pause");
	return (char)inb(COM1);
}


static int cellfs_ata_read(void *ctx, uint64_t lba, void *dst, uint32_t sectors) {
	(void)ctx;
	if (!dst || !sectors || lba > 0x0FFFFFFFu) return 0;
	return ata_pio_read28((uint32_t)lba, (uint8_t *)dst, sectors);
}

static int cellfs_ata_write(void *ctx, uint64_t lba, const void *src, uint32_t sectors) {
	(void)ctx;
	if (!src || !sectors || lba > 0x0FFFFFFFu) return 0;
	return ata_pio_write28((uint32_t)lba, (const uint8_t *)src, sectors);
}

static void put_dec_u64(uint64_t v) {
	char b[24]; unsigned n=0;
	if (!v) { dbg_putchar('0'); return; }
	while (v && n < sizeof(b)) { b[n++] = (char)('0' + v % 10u); v /= 10u; }
	while (n) dbg_putchar(b[--n]);
}

void cortex_boot(const handoff_t *ho) {
	const cell_boot_ext_t *ext = ho ? (const cell_boot_ext_t *)(uintptr_t)ho->reserved : 0;
	if (!ext || ext->magic != CELL_BOOT_EXT_MAGIC || ext->version != CELL_BOOT_EXT_VERSION) {
		dbg_puts("#CORTEX boot extension missing\n");
		return;
	}
	if (!(ext->flags & CELL_BOOT_EXT_F_MODEL) || !ext->model_lba || !ext->model_bytes) {
		dbg_puts("#CORTEX no model on boot medium\n");
		return;
	}
	cell_mem_arena_t arena;
	if (!cell_mem_arena_init(&arena, ho)) {
		dbg_puts("#CORTEX memory arena failed\n");
		return;
	}
	uint64_t disk_bytes = (ext->model_bytes + 511u) & ~511ull;
	void *blob = cell_mem_alloc(&arena, (size_t)disk_bytes, 4096u);
	if (!blob) { dbg_puts("#CORTEX model allocation failed\n"); return; }
	dbg_puts("#CORTEX loading bytes="); put_dec_u64(ext->model_bytes); dbg_puts("\n");
	if (!ata_pio_read_bytes(ext->model_lba, blob, ext->model_bytes)) {
		dbg_puts("#CORTEX ATA model read failed\n");
		return;
	}
	cwm_model_t model;
	if (!cwm_open(&model, blob, (size_t)ext->model_bytes)) {
		dbg_puts("#CORTEX CWM validation failed\n");
		return;
	}
	size_t workspace_bytes = cortex_workspace_bytes(&model);
	void *workspace = cell_mem_alloc(&arena, workspace_bytes, 64u);
	if (!workspace) { dbg_puts("#CORTEX workspace allocation failed\n"); return; }
	cortex_t cortex;
	if (!cortex_init(&cortex, &model, workspace, workspace_bytes)) {
		dbg_puts("#CORTEX inference init failed\n");
		return;
	}
	cell_vfs_t *vfs = 0;
	if ((ext->flags & CELL_BOOT_EXT_F_CELLFS) && ext->cellfs_sectors > CELLFS_DATA_LBA) {
		vfs = (cell_vfs_t *)cell_mem_alloc(&arena, sizeof(*vfs), 64u);
		if (vfs) {
			uint64_t model_sectors = (ext->model_bytes + 511u) / 512u;
			uint64_t cellfs_lba = ext->model_lba + model_sectors;
			cellfs_disk_t disk = {
				.read = cellfs_ata_read,
				.write = cellfs_ata_write,
				.ctx = 0,
				.base_lba = cellfs_lba,
				.total_sectors = ext->cellfs_sectors
			};
			if (!cell_vfs_mount(vfs, &disk, ext->model_bytes, model.h->vocab_size,
			    model.h->context_len, model.h->d_model, model.h->n_layers)) vfs = 0;
		}
	}
	if (vfs) {
		dbg_puts("#CELLFS READY sectors=");
		put_dec_u64(ext->cellfs_sectors);
		dbg_puts(" cwd=/\n");
	} else {
		dbg_puts("#CELLFS unavailable\n");
	}
	cell_task_manager_t *tasks = 0;
	if (vfs) {
		tasks = (cell_task_manager_t *)cell_mem_alloc(&arena, sizeof(*tasks), 64u);
		if (tasks) cell_task_manager_init(tasks, cell_task_default_policy());
	}
	cell_capability_env_t cap_env = {
		.handoff = ho,
		.boot_ext = ext,
		.arena = &arena,
		.cortex_ready = 1,
		.ata0_ready = 1,
		.vfs = vfs,
		.tasks = tasks
	};
	dbg_puts("#CORTEX READY vocab="); put_dec_u64(model.h->vocab_size);
	dbg_puts(" ctx="); put_dec_u64(model.h->context_len);
	dbg_puts(" d="); put_dec_u64(model.h->d_model);
	dbg_puts(" layers="); put_dec_u64(model.h->n_layers);
	dbg_puts("\n#CAPABILITY dispatcher ready\n");
	dbg_puts(tasks ? "#TASK executor ready\n" : "#TASK executor unavailable\n");

	for (;;) {
		char line[CORTEX_MAX_LINE]; size_t n=0;
		dbg_puts("cell$ ");
		for (;;) {
			char ch=serial_getc();
			if (ch=='\r' || ch=='\n') { dbg_puts("\n"); break; }
			if ((ch=='\b' || ch==127) && n) { --n; dbg_puts("\b \b"); continue; }
			if (ch >= 32 && ch < 127 && n + 1u < sizeof(line)) { line[n++]=ch; dbg_putchar(ch); }
		}
		line[n]=0;
		if (!n) continue;
		cortex_session_result_t result;
		if (!cortex_session_process(&cortex,line,&cap_env,&result)) {
			dbg_puts("Cell Cortex could not complete that request.\n");
			continue;
		}
		if (result.response[0]) {
			dbg_puts(result.response);
			dbg_puts("\n");
		}
	}
}
