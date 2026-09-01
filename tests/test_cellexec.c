/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/cellexec.h"

static uint8_t blob[512];
static uint64_t known_mask(void) {
	uint64_t m = 0; for (unsigned i = 1; i <= 11; ++i) m |= 1ull << i; return m;
}
static size_t make_exec(const cell_exec_insn_t *code, uint32_t n,
	const uint8_t *data, uint32_t data_bytes, uint64_t caps, uint32_t gas) {
	memset(blob, 0, sizeof(blob));
	cell_exec_header_t *h = (cell_exec_header_t *)blob;
	h->magic=CELL_EXEC_MAGIC; h->version=CELL_EXEC_VERSION; h->header_bytes=CELL_EXEC_HEADER_BYTES;
	h->instruction_bytes=CELL_EXEC_INSN_BYTES; h->code_bytes=n*CELL_EXEC_INSN_BYTES;
	h->data_bytes=data_bytes; h->entry_pc=0; h->capability_mask=caps; h->memory_bytes=64;
	h->gas_limit=gas; h->flags=0; h->total_bytes=CELL_EXEC_HEADER_BYTES+h->code_bytes+data_bytes;
	memcpy(blob+CELL_EXEC_HEADER_BYTES,code,h->code_bytes);
	if(data_bytes) memcpy(blob+CELL_EXEC_HEADER_BYTES+h->code_bytes,data,data_bytes);
	h->payload_crc32=cell_exec_crc32(blob+CELL_EXEC_HEADER_BYTES,h->code_bytes+data_bytes);
	return h->total_bytes;
}
static int expect(cell_exec_status_t got, cell_exec_status_t want, const char *name) {
	if (got==want) return 1;
	fprintf(stderr,"FAIL %s: got=%s want=%s\n",name,cell_exec_status_name(got),cell_exec_status_name(want));
	return 0;
}
int main(void) {
	int ok=1; cell_exec_t x;
	const uint8_t hello[]="hello\n";
	cell_exec_insn_t good[]={{CELL_EXEC_OP_PUTS,0,6,0,0},{CELL_EXEC_OP_HALT,0,0,0,0}};
	size_t n=make_exec(good,2,hello,6,0,16);
	ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_OK,"valid");
	blob[n-1]^=1; ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_BAD_CRC,"crc"); blob[n-1]^=1;
	cell_exec_insn_t branch[]={{CELL_EXEC_OP_JMP,0,0,0,99},{CELL_EXEC_OP_HALT,0,0,0,0}};
	n=make_exec(branch,2,0,0,0,16); ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_BAD_BRANCH,"branch");
	cell_exec_insn_t undeclared[]={{CELL_EXEC_OP_CAP,0,0,0,3},{CELL_EXEC_OP_HALT,0,0,0,0}};
	n=make_exec(undeclared,2,0,0,0,16); ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_BAD_CAPABILITY,"undeclared capability");
	n=make_exec(undeclared,2,0,0,1ull<<3,16); ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_OK,"declared capability");
	cell_exec_header_t *h=(cell_exec_header_t*)blob; h->capability_mask|=1ull<<63;
	h->payload_crc32=cell_exec_crc32(blob+64,h->code_bytes+h->data_bytes);
	ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_BAD_CAPABILITY,"unknown cap bit");
	cell_exec_insn_t badop[]={{255,0,0,0,0}}; n=make_exec(badop,1,0,0,0,16);
	ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_BAD_OPCODE,"opcode");
	n=make_exec(good,2,hello,6,0,0); ok &= expect(cell_exec_open(&x,blob,n,known_mask()),CELL_EXEC_BAD_GAS,"zero gas");
	if(!ok) return 1;
	puts("#CELLEXEC ABI validation PASS");
	puts("#CELLEXEC CRC/control-flow validation PASS");
	puts("#CELLEXEC capability declaration validation PASS");
	return 0;
}
