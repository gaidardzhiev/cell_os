# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#

.globl _start
.text
.align 3
_start:
	ldr x0, =stack_top
	mov sp, x0
	bl marker_main
1:  wfi
	b 1b

.bss
.balign 16
stack_area:
.space 4096
stack_top:
.section .note.GNU-stack,"",@progbits
