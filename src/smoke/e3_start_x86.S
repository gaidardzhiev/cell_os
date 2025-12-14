# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.section .bss
.align 4096
e3_pml4:
.skip 4096
.align 4096
e3_pdpt:
.skip 4096
.align 4096
e3_pd:
.skip 4096
.align 16
e3_stack:
.skip 8192
e3_stack_top:
	.macro COM1_PUTC ch
1:
	mov $0x3FD, %dx
	in %dx, %al
	test $0x20, %al
	jz 1b
	mov $0x3F8, %dx
	mov $\ch, %al
	out %al, (%dx)
	.endm
	.text
	.globl _start
	.globl e3_start
	.type e3_start, @function
_start:
	jmp e3_start
e3_start:
	lea e3_stack_top(%rip), %rsp
	lea e3_pml4(%rip), %rdi
	xor %eax, %eax
	mov $0x600, %rcx            
	rep stosq
	lea e3_pdpt(%rip), %rax
	or $0x3, %rax
	mov %rax, e3_pml4(%rip)
	lea e3_pd(%rip), %rax
	or $0x3, %rax
	mov %rax, e3_pdpt(%rip)
	lea e3_pd(%rip), %rbx
	xor %ecx, %ecx
1:
	mov %rcx, %rax
	shl $21, %rax               
	or $0x83, %rax              
	mov %rax, (%rbx,%rcx,8)
	inc %ecx
	cmp $16, %ecx               
	jl 1b
	mov %cr4, %rax
	or $0x10, %rax              
	mov %rax, %cr4
	lea e3_pml4(%rip), %rax
	mov %rax, %cr3
	mov $0x3F8, %dx
	xor %eax, %eax
	out %al, (%dx)              
	mov $0x3FB, %dx
	mov $0x80, %al
	out %al, (%dx)              
	mov $0x3F8, %dx
	mov $0x01, %al
	out %al, (%dx)              
	mov $0x3F9, %dx
	xor %al, %al
	out %al, (%dx)              
	mov $0x3FB, %dx
	mov $0x03, %al
	out %al, (%dx)
	mov $0x3FA, %dx
	mov $0xC7, %al
	out %al, (%dx)              
	mov $0x3FC, %dx
	mov $0x0B, %al
	out %al, (%dx)              
	COM1_PUTC '#'
	COM1_PUTC 'E'
	COM1_PUTC '3'
	COM1_PUTC 'i'
	COM1_PUTC '\n'
	mov $0xE9, %dx
	mov $'#', %al
	out %al, (%dx)
	mov $'E', %al
	out %al, (%dx)
	mov $'3', %al
	out %al, (%dx)
	mov $'i', %al
	out %al, (%dx)
	mov $'\n', %al
	out %al, (%dx)
	call e3_main

.Lhang:
hlt
jmp .Lhang
.section .note.GNU-stack,"",@progbits
