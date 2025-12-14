# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.section .text
.global _start
.extern kernel64_main
.extern __kernel_phys_base
.extern __kernel_phys_end
.extern __k_bss_start
.extern __k_bss_end
.extern cgf_e0_probe_arm64
.equ UART_BASE, 0x09000000
.equ DR,  0x00
.equ FR,  0x18
.equ IBRD,0x24
	.equ FBRD,0x28
	.equ LCRH,0x2C
	.equ CR,  0x30

_start:
	mrs x5, CurrentEL
	cmp x5, #0x8
	b.ne el1_entry
	ldr x0, =0x40020000
	msr SP_EL1, x0
	mov x0, #(1 << 31)
	msr HCR_EL2, x0
	mov x0, #3
	msr CNTHCTL_EL2, x0
	msr CNTVOFF_EL2, xzr
	mov x0, #0x3C5
	msr SPSR_EL2, x0
	adr x0, el1_entry
	msr ELR_EL2, x0
	eret

el1_entry:
	ldr x0, =0x40020000
	mov sp, x0
	mrs x1, CPACR_EL1
	orr x1, x1, #(3 << 20)
	msr CPACR_EL1, x1
	isb
	ldr x3, =__k_bss_start
	ldr x4, =__k_bss_end
1:
	cmp x3, x4
	b.hs 2f
	str xzr, [x3], #8
	b 1b
2:
	ldr x1, =UART_BASE
	str wzr, [x1, #CR]
	mov w2, #13
	str w2, [x1, #IBRD]
	mov w2, #2
	str w2, [x1, #FBRD]
	mov w2, #(3 << 5)
	str w2, [x1, #LCRH]
	mov w2, #(1 | (1 << 8) | (1 << 9))
	str w2, [x1, #CR]
	ldr x2, =msg_pl011
1:  ldrb w3, [x2], #1
	cbz  w3, 2f
3:  ldr  w4, [x1, #FR]
	tbz  w4, #5, 4f
	b    3b
4:  str  w3, [x1, #DR]
	b    1b
2:
	sub sp, sp, #168
	mov x0, sp
	mov x19, x0
	mov x1, #21
	mov x2, x0
1:
	str xzr, [x2], #8
	subs x1, x1, #1
	b.ne 1b
	ldr x2, =__kernel_phys_base
	str x2, [x0, #0]
	ldr x3, =__kernel_phys_end
	sub x3, x3, x2
	str x3, [x0, #8]
	mov x4, #0x04000000
	str x4, [x0, #16]
	mov x4, #4
	str x4, [x0, #24]
	mov x4, #0
	str x4, [x0, #32]
	mov x0, x19
	bl cgf_e0_probe_arm64
	mov x0, x19
	bl kernel64_main

.hang:
	wfi
	b .hang

.section .rodata
msg_pl011: .ascii "#E0 arm pl011 ok\n\0"

	.section .note.GNU-stack,"",@progbits
