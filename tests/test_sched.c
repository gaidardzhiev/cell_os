/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "core/channel.h"
#include "core/scheduler.h"

struct vm_handle {
	int id;
};

typedef struct {
	struct vm_handle handle;
	uint16_t chan_script[4];
	uint32_t cost_script[4];
	size_t script_len;
	size_t script_idx;
	uint16_t last_chan;
	uint32_t last_cost;
	int run_calls;
	int has_call;
} fake_vm_t;

static fake_vm_t vm0 = {
	.handle = { .id = 0 },
	.chan_script = { 0, 0 },
	.cost_script = { 256, 256 },
	.script_len = 2,
	.script_idx = 0
};

static fake_vm_t vm1 = {
	.handle = { .id = 1 },
	.chan_script = { 0 },
	.cost_script = { 256 },
	.script_len = 1,
	.script_idx = 0
};

static fake_vm_t* all_vms[] = { &vm0, &vm1 };

static const size_t vm_count = sizeof(all_vms) / sizeof(all_vms[0]);

static fake_vm_t* find_vm(vm_handle_t* vm) {
	for (size_t i = 0; i < vm_count; ++i) {
		if (&all_vms[i]->handle == vm) {
			return all_vms[i];
		}
	}
	return NULL;
}

bool vm_run_quantum(vm_handle_t* vm, uint64_t quantum_ns) {
	(void)quantum_ns;
	fake_vm_t* st = find_vm(vm);
	if (!st)
		return false;
	st->run_calls++;
	if (st->script_idx < st->script_len) {
	st->last_chan = st->chan_script[st->script_idx];
	st->last_cost = st->cost_script[st->script_idx];
	st->script_idx++;
	st->has_call = 1;
	} else {
		st->has_call = 0;
	}
	return true;
}

bool vm_has_syscall(vm_handle_t* vm) {
	fake_vm_t* st = find_vm(vm);
	if (!st)
		return false;
	if (st->has_call) {
		st->has_call = 0;
		return true;
	}
	return false;
}

uint16_t vm_last_channel(vm_handle_t* vm) {
	fake_vm_t* st = find_vm(vm);
	return st ? st->last_chan : 0;
}

uint32_t vm_last_syscall_cost_bytes(vm_handle_t* vm) {
	fake_vm_t* st = find_vm(vm);
	return st ? st->last_cost : 0;
}

static void test_empty_graph(void) {
	channel_graph_t g = {0};
	scheduler_t sched = {0};
	sched_init(&sched, &g, 1000);
	sched_tick(&sched);
	assert(sched_last_status() == QOS_OK);
}

int main(void) {
	test_empty_graph();
	qos_init(1);
	qos_cfg_channel(0, QOS_BE, 1, 1024, 512);
	qos_set_gas(1024);
	channel_qos_t qos_entries[1] = {
		{
			.channel_id = 0,
			.qos = QOS_BE,
			.priority = 0,
			.bucket = 1024,
			.bucket_max = 1024,
			.refill_per_tick = 512,
		},
	};
	uint8_t adj_bits[1] = { 0x01 };
	channel_graph_t cg = {
	.num_channels = 1,
	.qos = qos_entries,
	.adj = adj_bits,
	};
	scheduler_t sched;
	sched_init(&sched, &cg, 1000);
	gene_task_t task0 = {
		.gene_id = 1,
		.vm = &vm0.handle,
		.budget = { .cpu_budget_ns = 5000, .priority = 0, .runnable = 1 }
	};
	gene_task_t task1 = {
	.gene_id = 2,
	.vm = &vm1.handle,
	.budget = { .cpu_budget_ns = 5000, .priority = 1, .runnable = 1 }
	};
	sched_add(&task0);
	sched_add(&task1);
	sched_tick(&sched);
	assert(sched_last_status() == QOS_OK);
	assert(vm0.run_calls == 1);
	sched_tick(&sched);
	assert(sched_last_status() == QOS_BUSY);
	assert(vm1.run_calls == 1);
	assert(task1.budget.runnable == 0);
	qos_tick();
	qos_set_gas(0);
	sched_tick(&sched);
	assert(sched_last_status() == QOS_EGAS);
	assert(task0.budget.runnable == 0);
	puts("#E3 test sched ok");
	return 0;
}
