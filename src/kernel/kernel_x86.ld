ENTRY(_start)

PHDRS {
  text PT_LOAD FLAGS(5);
  ro   PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
  stack PT_GNU_STACK FLAGS(6);
}

SECTIONS {
  . = 0xFFFFFFFF80100000;

  .text : ALIGN(0x1000) {
	*(.text.boot*) *(.text*) *(.init*) *(.plt*)
  } :text

  .rodata : ALIGN(0x1000) {
	*(.rodata*) *(.eh_frame*)
  } :ro

  .data : ALIGN(0x1000) {
	*(.data*) *(.got*) *(.gcc_except_table*)
  } :data

  .bss (NOLOAD) : ALIGN(0x1000) {
	*(.bss*) *(COMMON)
  } :data

  __k_text_start = ADDR(.text);
  __k_text_end   = ADDR(.text)   + SIZEOF(.text);
  __k_ro_start   = ADDR(.rodata);
  __k_ro_end     = ADDR(.rodata) + SIZEOF(.rodata);
  __k_rw_start   = ADDR(.data);
  __k_rw_end     = ADDR(.data)   + SIZEOF(.data);
  __k_bss_start  = ADDR(.bss);
  __k_bss_end    = ADDR(.bss)    + SIZEOF(.bss);
}
