; Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
; SPDX-License-Identifier: MIT
;
; This file is licensed under the MIT License.
; See the LICENSE file in the project root for full license text.

org 0x7C00
bits 16

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00
	mov [boot_drive], dl
	mov si, dap
	mov ah, 0x42
	mov dl, [boot_drive]
	int 0x13
	jc disk_fail
	jmp 0x0000:0x8000

disk_fail:
	mov ah, 0x0E
	mov al, '#'
	int 0x10
	mov al, 'E'
	int 0x10
	mov al, '0'
	int 0x10
	mov al, '!'
	int 0x10
	jmp $

boot_drive: db 0

dap:
	db 16
	db 0
dap_sectors:	dw 0xA55A
dap_buffer:	dw 0x8000
		dw 0x0000
dap_lba:	dq 0x1122334455667788

times 510-($-$$) db 0
dw 0xAA55

section .note.GNU-stack
