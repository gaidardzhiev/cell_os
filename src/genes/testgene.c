/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gene/testgene.h"
#include "spec/cgf_substrate_e0.h"
#include "kernel/console.h"

void testgene_express(const handoff_t* ho) {
	if (!ho) {
		return;
	}
	if (ho->substrate.arch == CGF_SUBSTRATE_ARCH_X86_64) {
		dbg_puts("#E0 TestGene: I am expressed on x86_64\n");
		return;
	}
	if (ho->substrate.arch == CGF_SUBSTRATE_ARCH_ARM64) {
		dbg_puts("#E0 TestGene: I am expressed on ARM64\n");
		return;
	}
	dbg_puts("#E0 TestGene: unknown substrate arch\n");
}
