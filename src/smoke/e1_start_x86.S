# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#

.globl _start
.text
.code64
_start:
	lea stack_top(%rip), %rsp
	xor %rbp, %rbp
	call e1_main
1:  hlt
	jmp 1b

.bss
.balign 16
stack_area:
.space 4096
stack_top:
.section .note.Xen,"a",@note
.align 4
.long 4                         
.long 4                         
.long 0x12                      
.asciz "Xen"
.align 4
.long _start
.section .note.GNU-stack,"",@progbits
