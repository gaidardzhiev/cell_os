/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stddef.h>
#include "core/scheduler.h"
#include "core/organelles.h"

#ifndef SCHED_MAX_TASKS
#define SCHED_MAX_TASKS 32
#endif

static gene_task_t* g_tasks[SCHED_MAX_TASKS];
static uint32_t g_task_count;
static uint32_t g_curr_index;
static qos_status_t g_last_status = QOS_OK;
extern bool vm_run_quantum(vm_handle_t* vm, uint64_t quantum_ns);
extern bool vm_has_syscall(vm_handle_t* vm);
extern uint16_t vm_last_channel(vm_handle_t* vm);
extern uint32_t vm_last_syscall_cost_bytes(vm_handle_t* vm);

static void mark_dead(gene_task_t* t) {
	if (!t) {
		return;
	}
	t->budget.runnable = 0;
	organelles_mark_dead(t->gene_id);
}

void sched_init(scheduler_t* s, const channel_graph_t* cg, uint64_t quantum_ns) {
	if (!s) {
		return;
	}
	s->quantum_ns = quantum_ns;
	s->cg = cg;
	organelles_init(cg);
	for (uint32_t i = 0; i < SCHED_MAX_TASKS; ++i) {
		g_tasks[i] = NULL;
	}
	g_task_count = 0;
	g_curr_index = 0;
	g_last_status = QOS_OK;
}

void sched_add(gene_task_t* t) {
	if (!t || g_task_count >= SCHED_MAX_TASKS) {
		return;
	}
	g_tasks[g_task_count++] = t;
}

static void handle_task(gene_task_t* t, scheduler_t* s) {
	if (!t || !t->budget.runnable) {
		return;
	}
	if (t->budget.cpu_budget_ns != 0 && t->budget.cpu_budget_ns < s->quantum_ns) {
		mark_dead(t);
		return;
	}
	if (t->budget.cpu_budget_ns != 0) {
		t->budget.cpu_budget_ns -= s->quantum_ns;
	}
	g_last_status = QOS_OK;
	if (!vm_run_quantum(t->vm, s->quantum_ns)) {
		mark_dead(t);
		return;
	}
	organelles_charge_cpu(t->gene_id, s->quantum_ns);
	if (vm_has_syscall(t->vm)) {
		uint16_t chan = vm_last_channel(t->vm);
		uint32_t cost = vm_last_syscall_cost_bytes(t->vm);
		if (!organelles_route_allowed(chan, chan)) {
			g_last_status = QOS_BUSY;
			mark_dead(t);
			return;
		}
		g_last_status = qos_try_enqueue(chan, cost);
		if (g_last_status != QOS_OK) {
			mark_dead(t);
			return;
		}
		organelles_charge_io(t->gene_id, cost);
		organelles_charge_mem(t->gene_id, cost);
	}
}

void sched_tick(scheduler_t* s) {
	if (!s || g_task_count == 0) {
		g_last_status = QOS_OK;
		return;
	}
	for (uint32_t i = 0; i < g_task_count; ++i) {
		gene_task_t* t = g_tasks[g_curr_index];
		g_curr_index = (g_curr_index + 1) % g_task_count;
		if (!t || !t->budget.runnable) {
			continue;
		}
		handle_task(t, s);
		return;
	}
	g_last_status = QOS_BUSY;
}

qos_status_t sched_last_status(void) {
	return g_last_status;
}
