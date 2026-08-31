#include <stddef.h>
#include <stdint.h>
#include "boot/boot_ext.h"
#include "core/handoff.h"
#include "core/mem_arena.h"
#include "cortex/cwm.h"
#include "cortex/cortex.h"
#include "drivers/ata_pio.h"
#include "kernel/console.h"

#define COM1 0x3F8u
#define CORTEX_MAX_LINE 192u
#define CORTEX_MAX_GEN 48u

static inline uint8_t inb(uint16_t port) {
	uint8_t v;
	__asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port));
	return v;
}

static char serial_getc(void) {
	while (!(inb(COM1 + 5u) & 1u)) __asm__ volatile("pause");
	return (char)inb(COM1);
}

static void put_dec_u64(uint64_t v) {
	char b[24]; unsigned n=0;
	if (!v) { dbg_putchar('0'); return; }
	while (v && n < sizeof(b)) { b[n++] = (char)('0' + v % 10u); v /= 10u; }
	while (n) dbg_putchar(b[--n]);
}

static void feed_literal(cortex_t *c, const char *s) {
	while (*s && c->pos < c->model.h->context_len - 1u) cortex_feed(c, (uint8_t)*s++);
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
	dbg_puts("#CORTEX READY vocab="); put_dec_u64(model.h->vocab_size);
	dbg_puts(" ctx="); put_dec_u64(model.h->context_len);
	dbg_puts(" d="); put_dec_u64(model.h->d_model);
	dbg_puts(" layers="); put_dec_u64(model.h->n_layers);
	dbg_puts("\n");

	for (;;) {
		char line[CORTEX_MAX_LINE]; size_t n=0;
		dbg_puts("cell> ");
		for (;;) {
			char ch=serial_getc();
			if (ch=='\r' || ch=='\n') { dbg_puts("\n"); break; }
			if ((ch=='\b' || ch==127) && n) { --n; dbg_puts("\b \b"); continue; }
			if (ch >= 32 && ch < 127 && n + 1u < sizeof(line)) { line[n++]=ch; dbg_putchar(ch); }
		}
		line[n]=0;
		if (!n) continue;
		cortex_reset(&cortex);
		feed_literal(&cortex,"cell> ");
		for (size_t i=0;i<n && cortex.pos < model.h->context_len-1u;++i) cortex_feed(&cortex,(uint8_t)line[i]);
		if (cortex.pos < model.h->context_len-1u) cortex_feed(&cortex,(uint8_t)'\n');
		for (unsigned i=0;i<CORTEX_MAX_GEN && cortex.pos < model.h->context_len-1u;++i) {
			uint8_t t=cortex_next(&cortex);
			if (t==0) break;
			dbg_putchar((char)t);
			if (!cortex_feed(&cortex,t) || t=='\n') break;
		}
		dbg_puts("\n");
	}
}
