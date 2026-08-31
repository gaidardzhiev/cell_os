# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
.text
.globl _start
.extern kernel64_main
_start:
	/* Stage2 enters with SysV entry alignment (%rsp % 16 == 8). */
	sub $8, %rsp
	call kernel64_main
	add $8, %rsp
1:
	hlt
	jmp 1b
.section .note.GNU-stack,"",@progbits
