# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.section .text
.align 11
.global pf12_vectors
.global pf12_install_vectors
.extern pf12_irq_handler

pf12_vectors:

	b .
	b pf12_irq_entry
	b pf12_irq_entry
	b .

	b .
	b pf12_irq_entry
	b pf12_irq_entry
	b .

	b .
	b pf12_irq_entry
	b pf12_irq_entry
	b .

	b .
	b pf12_irq_entry
	b pf12_irq_entry
	b .

.align 7
pf12_irq_entry:
	sub sp, sp, #(32*8)
	stp x0, x1, [sp, #0]
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]
	stp x6, x7, [sp, #48]
	stp x8, x9, [sp, #64]
	stp x10, x11, [sp, #80]
	stp x12, x13, [sp, #96]
	stp x14, x15, [sp, #112]
	stp x16, x17, [sp, #128]
	stp x18, x19, [sp, #144]
	stp x20, x21, [sp, #160]
	stp x22, x23, [sp, #176]
	stp x24, x25, [sp, #192]
	stp x26, x27, [sp, #208]
	stp x28, x29, [sp, #224]
	stp x30, xzr, [sp, #240]
	bl pf12_irq_handler
	ldp x30, xzr, [sp, #240]
	ldp x28, x29, [sp, #224]
	ldp x26, x27, [sp, #208]
	ldp x24, x25, [sp, #192]
	ldp x22, x23, [sp, #176]
	ldp x20, x21, [sp, #160]
	ldp x18, x19, [sp, #144]
	ldp x16, x17, [sp, #128]
	ldp x14, x15, [sp, #112]
	ldp x12, x13, [sp, #96]
	ldp x10, x11, [sp, #80]
	ldp x8, x9, [sp, #64]
	ldp x6, x7, [sp, #48]
	ldp x4, x5, [sp, #32]
	ldp x2, x3, [sp, #16]
	ldp x0, x1, [sp, #0]
	add sp, sp, #(32*8)
	eret

pf12_install_vectors:
	adr x0, pf12_vectors
	msr VBAR_EL1, x0
	isb
	ret

.section .note.GNU-stack,"",@progbits
