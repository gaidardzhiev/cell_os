/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

OUTPUT_FORMAT(elf64-littleaarch64)
ENTRY(_start)

SECTIONS
{
  . = 0x40080000;

  .text : {
	*(.text*)
  }

  .rodata : {
	*(.rodata*)
  }

  .data : {
	*(.data*)
  }

  .bss (NOLOAD) : {
	*(.bss*)
	*(COMMON)
  }

  /DISCARD/ : {
	*(.comment*)
	*(.eh_frame*)
  }
}
