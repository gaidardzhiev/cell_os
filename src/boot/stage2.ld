/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */


ENTRY(start16)

SECTIONS
{
	. = 0x8000;
	.text : { *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }
}
