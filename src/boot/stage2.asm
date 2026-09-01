; Cell OS stage2, Cortex branch
; Based on cell_os commit 515d65c738e17163fe9419d75f4841aefefc32ef
; Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
; SPDX-License-Identifier: GPL-3.0-or-later
BITS 16
org 0x8000

%define E9_PORT 0xE9
%define COM1_PORT 0x3F8
%define STACK_TOP 0x98000
%define IDENT_LIMIT_MB 1024
%define KERNEL_LOAD_ADDR 0x00100000
%define KERNEL_VIRT_ADDR 0xFFFFFFFF80100000
%define KERNEL_PML4_HIGH (511*8)
%define KERNEL_PDPT_HIGH (510*8)
%define KERNEL_STAGE_ADDR 0x00020000
%define KERNEL_STAGE_SEG (KERNEL_STAGE_ADDR >> 4)
%define KERNEL_STAGE_OFF (KERNEL_STAGE_ADDR & 0xF)
%define E820_MAP_ADDR 0x5000
%define BOOT_EXT_ADDR 0x5400
%define MAX_E820_ENTRIES 32

%macro OUTDBG 1
	mov dx, E9_PORT
	mov al, %1
	out dx, al
	mov dx, COM1_PORT
	out dx, al
%endmacro

start16:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov [boot_drive_s2], dl
	cld
	call collect_e820

	; COM1: 115200 8N1, FIFO on.
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

	in al, 0x92
	or al, 00000010b
	out 0x92, al
	mov si, msg_a20
	call log_str16

	; Load the packed kernel to a low staging buffer while BIOS services exist.
	mov si, dap3
	mov ah, 0x42
	mov dl, [boot_drive_s2]
	int 0x13
	jc load_fail
	mov si, msg_kernel
	call log_str16

	lgdt [gdt_desc]
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	jmp 08h:prot32

load_fail:
	mov si, msg_load_fail
	call log_str16
	jmp $

collect_e820:
	pushad
	xor ebx, ebx
	mov di, E820_MAP_ADDR
	xor bp, bp
.e820_loop:
	cmp bp, MAX_E820_ENTRIES
	jae .e820_done
	mov eax, 0xE820
	mov edx, 0x534D4150
	mov ecx, 24
	mov dword [es:di+20], 1
	int 0x15
	jc .e820_done
	cmp eax, 0x534D4150
	jne .e820_done
	cmp ecx, 20
	jb .e820_done
	add di, 24
	inc bp
	test ebx, ebx
	jnz .e820_loop
.e820_done:
	mov [e820_count], bp
	popad
	ret

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

	; Enable native x87/SSE state. Cortex currently uses an SSE2 matvec path.
	mov eax, cr0
	and eax, 0xFFFFFFF3       ; clear EM and TS
	or eax, (1<<1) | (1<<5)  ; MP and NE
	mov cr0, eax

	mov esi, KERNEL_STAGE_ADDR
	mov edi, KERNEL_LOAD_ADDR
	mov ecx, dword [kernel_bytes]
	rep movsb
	mov esi, msg_prot32
	call log_str32

	mov eax, cr4
	or eax, (1<<5) | (1<<9) | (1<<10) ; PAE, OSFXSR, OSXMMEXCPT
	mov cr4, eax

	; One PML4 + one PDPT + one 2 MiB page directory maps 0..1 GiB,
	; and aliases the same physical range at the Cell OS high-half address.
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
	or eax, 0x083
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
	lea rsi, [rel msg_long]
	call log_str64

	; Highest usable E820 end that is reachable by our current 1 GiB map.
	xor r8, r8
	movzx rcx, word [e820_count]
	mov rsi, E820_MAP_ADDR
.mem_scan:
	test rcx, rcx
	jz .mem_done
	cmp dword [rsi+16], 1
	jne .mem_next
	mov rax, [rsi+0]
	mov rdx, [rsi+8]
	add rax, rdx
	jc .mem_next
	cmp rax, r8
	jbe .mem_next
	mov r8, rax
.mem_next:
	add rsi, 24
	dec rcx
	jmp .mem_scan
.mem_done:
	test r8, r8
	jnz .mem_have
	mov r8, 0x04000000
.mem_have:
	cmp r8, 0x40000000
	jbe .mem_capped
	mov r8, 0x40000000
.mem_capped:

	; Optional extension pointed to by handoff.reserved.  The frozen 168-byte
	; handoff ABI itself is unchanged.
	mov rdi, BOOT_EXT_ADDR
	xor rax, rax
	mov rcx, 6
	rep stosq
	mov dword [BOOT_EXT_ADDR+0], 0x31584243 ; CBX1
	mov word  [BOOT_EXT_ADDR+4], 1
	mov word  [BOOT_EXT_ADDR+6], 48
	mov qword [BOOT_EXT_ADDR+8], E820_MAP_ADDR
	movzx eax, word [e820_count]
	mov dword [BOOT_EXT_ADDR+16], eax
	mov dword [BOOT_EXT_ADDR+20], 24
	mov rax, [model_lba]
	mov [BOOT_EXT_ADDR+24], rax
	mov rax, [model_bytes]
	mov [BOOT_EXT_ADDR+32], rax
	test rax, rax
	jz .no_model
	or dword [BOOT_EXT_ADDR+40], 1
.no_model:
	mov eax, [cellfs_sectors]
	mov [BOOT_EXT_ADDR+44], eax
	test eax, eax
	jz .no_cellfs
	or dword [BOOT_EXT_ADDR+40], 2
.no_cellfs:

	sub rsp, 168
	xor rax, rax
	mov rcx, 21
	mov rdi, rsp
	rep stosq
	mov qword [rsp+0], KERNEL_LOAD_ADDR
	mov rax, [kernel_bytes]
	mov [rsp+8], rax
	mov [rsp+16], r8
	mov qword [rsp+24], 0x0000000000000003
	mov qword [rsp+32], BOOT_EXT_ADDR
	mov rdi, rsp

	; 168-byte handoff leaves rsp%16==8. Restore SysV pre-call alignment.
	sub rsp, 8
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
pd: times 512 dq 0

boot_drive_s2: db 0
e820_count: dw 0

dap3:
	db 16, 0
dap3_secs: dw 0x5AA5
dap3_buf_off: dw KERNEL_STAGE_OFF
dap3_buf_seg: dw KERNEL_STAGE_SEG
dap3_lba: dq 0x8877665544332211

kernel_bytes: dq 0xCAFEBABEDEADBEEF
model_lba: dq 0x13579BDF2468ACE0
model_bytes: dq 0x0F1E2D3C4B5A6978
cellfs_sectors: dd 0xA1B2C3D4

msg_s2: db "#E0 s2 start", 10, 0
msg_a20: db "#E0 a20 ok", 10, 0
msg_kernel: db "#E0 kernel load ok", 10, 0
msg_long: db "#E0 long ok", 10, 0
msg_prot32: db "#E0 prot32 copy ok", 10, 0
msg_paging: db "#E0 paging on", 10, 0
msg_load_fail: db "#E0 load fail", 10, 0

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
