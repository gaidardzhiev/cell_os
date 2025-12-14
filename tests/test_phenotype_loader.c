/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/phenotype.h"
#include "core/scheduler.h"
#include "core/qos.h"

static size_t read_all(const char* path, unsigned char** out) {
	FILE* f = fopen(path, "rb");
	if (!f)
		return 0;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f);
		return 0;
	}
	long L = ftell(f);
	if (L <= 0) { fclose(f);
		return 0;
	}
	if (fseek(f, 0, SEEK_SET) != 0) { fclose(f);
		return 0;
	}
	unsigned char* buf = (unsigned char*)malloc((size_t)L);
	if (!buf) { fclose(f);
		return 0;
	}
	size_t n = fread(buf, 1, (size_t)L, f);
	fclose(f);
	if (n != (size_t)L) { free(buf);
		return 0; }
	*out = buf;
	return (size_t)L;
}

bool vm_run_quantum(vm_handle_t* vm, uint64_t quantum_ns) {
	(void)vm;
	(void)quantum_ns;
	return false;
}

bool vm_has_syscall(vm_handle_t* vm) {
	(void)vm;
	return false;
}

uint16_t vm_last_channel(vm_handle_t* vm) {
	(void)vm;
	return 0;
}

uint32_t vm_last_syscall_cost_bytes(vm_handle_t* vm) {
	(void)vm;
	return 0;
}

int main(void) {
	const char* path = "build/x86_bios.minimal.pbin";
	unsigned char* buf = NULL;
	size_t len = read_all(path, &buf);
	assert(len && "compile phenotypes first");
	phenotype_bin_t pb;
	assert(phenotype_parse(buf, len, &pb));
	channel_graph_t cg;
	assert(phenotype_to_channel_graph(&pb, &cg));
	assert(cg.num_channels > 0);
	scheduler_t sched;
	sched_init(&sched, &cg, pb.hdr.quantum_ns);
	printf("#E4 phenotype->sched ok (N=%u, quantum=%u)\n", pb.hdr.num_channels, pb.hdr.quantum_ns);
	puts("#E4 phenotype loader ok");
	free(buf);
	return 0;
}
