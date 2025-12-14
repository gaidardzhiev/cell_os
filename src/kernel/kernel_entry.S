# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.text
.globl _start
.extern kernel64_main
.extern cgf_e0_probe_x86_64

_start:
	mov %rsp, %rsp        
	mov %rdi, %rdi        
	mov %rdi, %r12        
	call cgf_e0_probe_x86_64
	mov %r12, %rdi        
	call kernel64_main
1:
	hlt
	jmp 1b

.section .note.GNU-stack,"",@progbits
