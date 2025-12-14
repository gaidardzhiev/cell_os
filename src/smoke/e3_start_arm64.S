# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.globl _start
.text
.align 3
_start:
	ldr x0, =stack_top
	mov sp, x0
	bl e3_main
1:  wfi
	b 1b
.bss
.balign 16
stack_area:
.space 4096
stack_top:
.section .note.GNU-stack,"",@progbits
