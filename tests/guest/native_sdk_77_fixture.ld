/* The pinned acceptance fixture wraps the linked RX bytes in its existing
 * two-segment ELF format. This is not a general .tea linker or installer. */
ENTRY(_start)
SECTIONS {
 . = 0x10001000;
 .text : { KEEP(*(.text.entry)) *(.text .text.*) *(.rodata .rodata.*) }
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
 ASSERT(ADDR(.text) == 0x10001000, "entry page")
 ASSERT(SIZEOF(.text) <= 4096, "fixture RX page overflow")
 ASSERT(_start == 0x10001000, "entry address")
}
