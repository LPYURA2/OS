ENTRY(tea_768_start)
PHDRS { text PT_LOAD FLAGS(5); }
SECTIONS {
    . = 0x10000000;
    .text : { *(.text*) } :text
    /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) *(.rodata*) }
}
