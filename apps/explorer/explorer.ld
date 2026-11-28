ENTRY(_start)
SECTIONS {
 . = 0x10000000;
 .text : { KEEP(*(.text.entry)) *(.text .text.*) *(.rodata .rodata.*) }
 . = 0x12000000;
 .data : { BYTE(0); *(.data .data.*) *(.bss .bss.*) *(COMMON) }
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
 ASSERT(_start == 0x10000000, "entry")
 ASSERT(SIZEOF(.text) <= 32768, "code limit")
 ASSERT(SIZEOF(.data) <= 65536, "data limit")
}
