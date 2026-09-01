# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
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
