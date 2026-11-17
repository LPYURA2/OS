ENTRY(disk_app_entry)
SECTIONS {
    . = 0;
    .text : ALIGN(4) { KEEP(*(.text.app)) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) }
}
