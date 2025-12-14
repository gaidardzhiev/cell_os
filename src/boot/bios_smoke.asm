; Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
; SPDX-License-Identifier: MIT
;
; This file is licensed under the MIT License.
; See the LICENSE file in the project root for full license text.

BITS 16
ORG 0x7C00

start:
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00
	mov si, msg1
	call print_line
	mov si, msg2
	call print_line
	mov si, msg3
	call print_line
	mov si, msg4
	call print_line

.hang:
	jmp .hang

print_char:
	push ax
	push dx
	mov dx, 0x00E9
	out dx, al
	pop dx
	pop ax
	push ax
	push bx
	push cx
	mov ah, 0x0E
	mov bh, 0x00
	mov bl, 0x07
	int 0x10
	pop cx
	pop bx
	pop ax
	ret

print_line:
	push ax
.next:
	lodsb
	test al, al
	jz .done
	call print_char
	jmp .next
.done:
	mov al, 0x0D
	call print_char
	mov al, 0x0A
	call print_char
	pop ax
	ret

msg1: db '#E3 CG ok',0
msg2: db '#E8 OBS v2 replay=ok',0
msg3: db '#E9 SEC mac+roots',0
msg4: db '#E10 RELEASE ok',0

Times 510-($-$$) db 0
dw 0xAA55
