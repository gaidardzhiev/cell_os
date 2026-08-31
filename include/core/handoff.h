#pragma once
#include <stdint.h>

/* ABI-compatible prefix and total size of Cell OS handoff_t at base commit.
 * Cortex uses only the first five qwords; the final 128 bytes remain reserved
 * for the existing substrate descriptor.
 */
typedef struct __attribute__((packed)) {
	uint64_t phys_base;
	uint64_t phys_len;
	uint64_t mem_top;
	uint64_t flags;
	uint64_t reserved;
	uint8_t substrate[128];
} handoff_t;

_Static_assert(sizeof(handoff_t) == 168, "Cell OS handoff ABI must remain 168 bytes");
