; Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
; SPDX-License-Identifier: MIT
;
; This file is licensed under the MIT License.
; See the LICENSE file in the project root for full license text.

BITS 16
%ifidn __OUTPUT_FORMAT__,bin
org 0x8000
%endif

%define E9_PORT 0xE9
%define COM1_PORT 0x3F8
%define COM1_PORT 0x3F8
%define STACK_TOP 0x98000
%define IDENT_LIMIT_MB 64
%define KERNEL_LOAD_ADDR 0x00100000
%define KERNEL_VIRT_ADDR 0xFFFFFFFF80100000
%define KERNEL_PML4_HIGH (511*8)
%define KERNEL_PDPT_HIGH (510*8)
%define KERNEL_STAGE_ADDR 0x00020000
%define KERNEL_STAGE_SEG (KERNEL_STAGE_ADDR >> 4)
%define KERNEL_STAGE_OFF (KERNEL_STAGE_ADDR & 0xF)
%define CGF_STAGE_ADDR    0x00030000
%define CGF_STAGE_SEG (CGF_STAGE_ADDR >> 4)
%define CGF_STAGE_OFF (CGF_STAGE_ADDR & 0xF)

%ifdef CFG_ENABLE_CGF_VERIFY
extern cgf2_verify_and_log
%endif

global start16

%macro OUTDBG 1
	mov dx, E9_PORT
	mov al, %1
	out dx, al
	mov dx, COM1_PORT
	out dx, al
%endmacro

start16:
	cli
	mov [boot_drive_s2], dl
	cld
	mov dx, COM1_PORT + 1
	xor al, al
	out dx, al
	mov dx, COM1_PORT + 3
	mov al, 0x80
	out dx, al
	mov dx, COM1_PORT + 0
	mov al, 0x01
	out dx, al
	mov dx, COM1_PORT + 1
	xor al, al
	out dx, al
	mov dx, COM1_PORT + 3
	mov al, 0x03
	out dx, al
	mov dx, COM1_PORT + 2
	mov al, 0x07
	out dx, al
	mov dx, COM1_PORT + 4
	mov al, 0x0B
	out dx, al
	mov si, msg_s2
	call log_str16
	in   al, 0x92
	or   al, 00000010b
	out  0x92, al
	mov si, msg_a20
	call log_str16
	mov si, dap3
	mov ah, 0x42
	mov dl, [boot_drive_s2]
	int 0x13
	jc load_fail
	mov si, msg_kernel
	call log_str16
%ifdef CFG_ENABLE_CGF_VERIFY
	mov si, dap_cgf
	mov ah, 0x42
	mov dl, [boot_drive_s2]
	int 0x13
	jc cgf_load_fail
%endif
	lgdt [gdt_desc]
	mov eax, cr0
	or  eax, 1
	mov cr0, eax
	jmp 08h:prot32

load_fail:
	mov si, msg_load_fail
	call log_str16
	jmp $

%ifdef CFG_ENABLE_CGF_VERIFY
cgf_load_fail:
	mov si, msg_cgf_fail
	call log_str16
	jmp $
%endif

log_str16:
	pusha
.ls16_loop:
	lodsb
	test al, al
	jz .ls16_done
	OUTDBG al
	jmp .ls16_loop
.ls16_done:
	popa
	ret

[BITS 32]
prot32:
	mov ax, 10h
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, 0x90000
	cld
	mov eax, cr0
	or  eax, (1<<1) | (1<<5)
	mov cr0, eax
	mov esi, KERNEL_STAGE_ADDR
	mov edi, KERNEL_LOAD_ADDR
	mov ecx, dword [kernel_bytes]
	rep movsb
	mov esi, msg_prot32
	call log_str32
	mov eax, cr4
	or  eax, (1<<5) | (1<<9) | (1<<10)
	mov cr4, eax
	mov edi, pml4
	xor eax, eax
	mov ecx, (4096*3)/4
	rep stosd
	mov eax, pdpt
	or eax, 0x03
	mov [pml4], eax
	mov dword [pml4+4], 0
	mov [pml4 + KERNEL_PML4_HIGH], eax
	mov dword [pml4 + KERNEL_PML4_HIGH + 4], 0
	mov eax, pd
	or eax, 0x03
	mov [pdpt], eax
	mov dword [pdpt+4], 0
	mov [pdpt + KERNEL_PDPT_HIGH], eax
	mov dword [pdpt + KERNEL_PDPT_HIGH + 4], 0
	mov ecx, IDENT_LIMIT_MB / 2
	xor ebx, ebx
	mov edi, pd
.fill_pd:
	mov eax, ebx
	or  eax, 0x083
	mov [edi], eax
	mov dword [edi+4], 0
	add edi, 8
	add ebx, 0x200000
	loop .fill_pd
	mov eax, pml4
	mov cr3, eax
	mov ecx, 0xC0000080
	rdmsr
	or eax, (1<<8)
	wrmsr
	mov eax, cr0
	or eax, (1<<31)
	mov cr0, eax
	mov esi, msg_paging
	call log_str32
	jmp 18h:long64

log_str32:
	pushad
.ls32_loop:
	lodsb
	test al, al
	jz .ls32_done
	OUTDBG al
	jmp .ls32_loop
.ls32_done:
	popad
	ret

[BITS 64]
long64:
	cld
	mov ax, 0x20
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov rsp, STACK_TOP
	mov rbx, 0x0000000000108000
	mov rax, 0x1122334455667788
	mov [rbx], rax
	mov rax, [rbx]
	lea rsi, [rel msg_long]
	call log_str64
%ifdef CFG_ENABLE_CGF_VERIFY
	mov rdi, CGF_STAGE_ADDR
	mov rsi, [cgf_bytes]
	call cgf2_verify_and_log
	test eax, eax
	jnz .hang
%endif
	
	sub rsp, 168
	xor rax, rax
	mov rcx, 21
	mov rdi, rsp
	rep stosq
	mov qword [rsp+0], KERNEL_LOAD_ADDR
	mov rax, [kernel_bytes]
	mov [rsp+8], rax
	mov qword [rsp+16], 0x0000000004000000 
	mov qword [rsp+24], 0x0000000000000003 
	mov qword [rsp+32], 0
	mov rdi, rsp
	mov rax, KERNEL_VIRT_ADDR
	call rax

.hang:
	hlt
	jmp .hang

align 16
gdt:
	dq 0x0000000000000000
	dq 0x00CF9A000000FFFF
	dq 0x00CF92000000FFFF
	dq 0x00AF9A000000FFFF
	dq 0x00AF92000000FFFF
gdt_end:
gdt_desc:
	dw gdt_end - gdt - 1
	dd gdt

align 4096
pml4: times 512 dq 0
pdpt: times 512 dq 0
pd:   times 512 dq 0

boot_drive_s2: db 0

dap3:
	db 16, 0
dap3_secs:   dw 0x5AA5
dap3_buf_off dw KERNEL_STAGE_OFF
dap3_buf_seg dw KERNEL_STAGE_SEG
dap3_lba:    dq 0x8877665544332211

%ifdef CFG_ENABLE_CGF_VERIFY
dap_cgf:
	db 16, 0
cgf_secs:    dw 0xACE1
cgf_buf_off  dw CGF_STAGE_OFF
cgf_buf_seg  dw CGF_STAGE_SEG
cgf_lba:     dq 0x445566778899AABB
%endif

kernel_bytes: dq 0xCAFEBABEDEADBEEF
%ifdef CFG_ENABLE_CGF_VERIFY
cgf_bytes:    dq 0x0BADC0DED15EA5ED
%endif

msg_s2:     db "#E0 s2 start", 10, 0
msg_a20:    db "#E0 a20 ok", 10, 0
msg_kernel: db "#E0 kernel load ok", 10, 0
msg_long:   db "#E0 long ok", 10, 0
msg_prot32: db "#E0 prot32 copy ok", 10, 0
msg_paging: db "#E0 paging on", 10, 0

msg_load_fail: db "#E0 load fail", 10, 0
%ifdef CFG_ENABLE_CGF_VERIFY
msg_cgf_fail:  db "#A! cgf load fail", 10, 0
%endif

log_str64:
	push rax
	push rdx
	push rsi
.ls64_loop:
	lodsb
	test al, al
	jz .ls64_done
	OUTDBG al
	jmp .ls64_loop
.ls64_done:
	pop rsi
	pop rdx
	pop rax
	ret

section .note.GNU-stack
