# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#

.text
.globl cgf_cpuid_leaf0
.globl cgf_cpuid_leaf1
.globl cgf_cpuid_leaf7


cgf_cpuid_leaf0:
	push %rbx
	mov %rdx, %r9
	mov %rcx, %r8
	xor %eax, %eax
	cpuid
	test %rdi, %rdi
	jz .skip_eax0
	mov %eax, (%rdi)
.skip_eax0:
	test %rsi, %rsi
	jz .skip_ebx0
	mov %ebx, (%rsi)
.skip_ebx0:
	test %r9, %r9
	jz .skip_ecx0
	mov %ecx, (%r9)
.skip_ecx0:
	test %r8, %r8
	jz .done0
	mov %edx, (%r8)
.done0:
	pop %rbx
	ret


cgf_cpuid_leaf1:
	push %rbx
	mov %rdx, %r9
	mov %rcx, %r8
	mov $1, %eax
	xor %ecx, %ecx
	cpuid
	test %rdi, %rdi
	jz .skip_eax1
	mov %eax, (%rdi)
.skip_eax1:
	test %rsi, %rsi
	jz .skip_ebx1
	mov %ebx, (%rsi)
.skip_ebx1:
	test %r9, %r9
	jz .skip_ecx1
	mov %ecx, (%r9)
.skip_ecx1:
	test %r8, %r8
	jz .done1
	mov %edx, (%r8)
.done1:
	pop %rbx
	ret


cgf_cpuid_leaf7:
	push %rbx
	mov $7, %eax
	xor %ecx, %ecx
	cpuid
	test %rdi, %rdi
	jz .done7
	mov %ebx, (%rdi)
.done7:
	pop %rbx
	ret
