/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(e3_start)

PHDRS
{
	note PT_NOTE FLAGS(0);
	text PT_LOAD FLAGS(RWE);
}

SECTIONS
{
	. = 0;
	.note.Xen : {
		KEEP(*(.note.Xen))
} : note : text
. = 0x00100000;
. = ALIGN(16);
.text : {
	*(.text*)
} : text
.rodata : {
	*(.rodata*)
} : text
.data : {
	*(.data*)
} : text
.bss (NOLOAD) : {
	*(.bss*)
	*(COMMON)
} : text
/DISCARD/ : {
	*(.comment*)
	*(.eh_frame*)
	}
}
