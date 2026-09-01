/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include "core/mem_arena.h"
#include "boot/boot_ext.h"

#define CELL_KERNEL_VIRT_BASE 0xFFFFFFFF80100000ull
#define CELL_MAX_IDENTITY_PHYS 0x40000000ull

extern unsigned char __k_bss_end;

static uintptr_t align_up(uintptr_t v, uintptr_t a) {
	return (v + (a - 1u)) & ~(a - 1u);
}

static uint64_t min64(uint64_t a, uint64_t b) { return a < b ? a : b; }

int cell_mem_arena_init(cell_mem_arena_t *arena, const handoff_t *ho) {
	if (!arena || !ho) return 0;
	uintptr_t vend = (uintptr_t)&__k_bss_end;
	uint64_t kernel_end = ho->phys_base;
	if (vend >= CELL_KERNEL_VIRT_BASE)
		kernel_end += (uint64_t)(vend - CELL_KERNEL_VIRT_BASE);
	else
		kernel_end += ho->phys_len;
	kernel_end = align_up((uintptr_t)kernel_end, 4096u);

	uint64_t hard_end = min64(ho->mem_top, CELL_MAX_IDENTITY_PHYS);
	const cell_boot_ext_t *ext = (const cell_boot_ext_t *)(uintptr_t)ho->reserved;
	if (ext && ext->magic == CELL_BOOT_EXT_MAGIC &&
	    ext->version == CELL_BOOT_EXT_VERSION &&
	    ext->e820_entry_size == sizeof(cell_e820_entry_t) && ext->e820_ptr) {
		const cell_e820_entry_t *map = (const cell_e820_entry_t *)(uintptr_t)ext->e820_ptr;
		uint64_t best_base = 0, best_end = 0;
		for (uint32_t i = 0; i < ext->e820_count; ++i) {
			if (map[i].type != 1 || map[i].length == 0) continue;
			uint64_t b = map[i].base;
			uint64_t e = b + map[i].length;
			if (e < b) continue;
			if (b < kernel_end) b = kernel_end;
			if (e > hard_end) e = hard_end;
			b = align_up((uintptr_t)b, 4096u);
			if (e > b && e - b > best_end - best_base) {
				best_base = b;
				best_end = e;
			}
		}
		if (best_end > best_base) {
			arena->next = (uintptr_t)best_base;
			arena->end = (uintptr_t)best_end;
			return 1;
		}
	}
	if (hard_end <= kernel_end) return 0;
	arena->next = (uintptr_t)kernel_end;
	arena->end = (uintptr_t)hard_end;
	return 1;
}

void *cell_mem_alloc(cell_mem_arena_t *arena, size_t bytes, size_t alignment) {
	if (!arena || !bytes) return 0;
	if (alignment < 8) alignment = 8;
	if (alignment & (alignment - 1u)) return 0;
	uintptr_t p = align_up(arena->next, alignment);
	if (p > arena->end || bytes > (size_t)(arena->end - p)) return 0;
	arena->next = p + bytes;
	return (void *)p;
}

size_t cell_mem_available(const cell_mem_arena_t *arena) {
	return (!arena || arena->end < arena->next) ? 0 : (size_t)(arena->end - arena->next);
}
