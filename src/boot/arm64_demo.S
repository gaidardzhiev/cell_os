# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.section .text
.global _start
.align  7

.equ UART0, 0x09000000
.equ DR,    0x00
.equ FR,    0x18
.equ TXFF,  (1 << 5)

_start:
	ldr x0, =_stack_top
	mov sp, x0

	ldr x0, =msg1
	bl  puts
	ldr x0, =msg2
	bl  puts
	ldr x0, =msg3
	bl  puts
	ldr x0, =msg4
	bl  puts

1:  wfe
	b   1b

putc:
1:  ldr x1, =UART0
	ldr w2, [x1, #FR]
	tbnz w2, #5, 1b
	str w0, [x1, #DR]
	ret

puts:
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	mov x3, x0
2:  ldrb w0, [x3], #1
	cbz  w0, 3f
	bl   putc
	b    2b
3:
	mov w0, #0x0D
	bl  putc
	mov w0, #0x0A
	bl  putc
	ldp x29, x30, [sp], #16
	ret

	.section .rodata
msg1: .ascii "#E3 CG ok\0"
msg2: .ascii "#E8 OBS v2 replay=ok\0"
msg3: .ascii "#E9 SEC mac+roots\0"
msg4: .ascii "#E10 RELEASE ok\0"

	.section .bss
	.align 12
	.global _stack
_stack:
	.space 0x4000
	.global _stack_top
_stack_top:
