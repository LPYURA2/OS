OUTPUT_ARCH(aarch64)
ENTRY(image_start)

SECTIONS
{
    /* QEMU virt RAM begins at 0x40000000; Image text_offset is 0x80000. */
    . = 0x40080000;
    .text : ALIGN(8) {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }
    .rodata : ALIGN(16) { *(.rodata .rodata.*) }
    .data : ALIGN(16) { *(.data .data.*) }
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }
    __image_end = .;
    /DISCARD/ : { *(.comment .note.* .eh_frame .eh_frame_hdr) }
}

ASSERT((image_start & 7) == 0, "Image header alignment")
ASSERT((__stack_top & 15) == 0, "AArch64 stack alignment")
ASSERT((__bss_end & 7) == 0, "BSS clearing uses 64-bit stores")
