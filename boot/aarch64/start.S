/* QEMU virt ARM64 Image entry, using the documented AArch64 Linux boot ABI.
 * This header is a transport convention, not a Linux kernel dependency.
 * x0 is the physical DTB address. No firmware services are used here. */
.section .text.boot, "ax"
.balign 8
.global image_start
.global _start
image_start:
    b _start
    .long 0
    .quad 0x80000                 /* text_offset from QEMU virt RAM base */
    .quad __image_end - image_start
    .quad 0                      /* little endian, no page-size assumption */
    .quad 0
    .quad 0
    .quad 0
    .long 0x644d5241             /* ARM64 Image magic, bytes "ARM\x64" */
    .long 0

_start:
    msr daifset, #0xf
    mov x19, x0                 /* preserve DTB through EL transition */
    mrs x1, mpidr_el1
    and x1, x1, #0xffffff
    cbnz x1, .Lpark             /* prototype boots a single CPU */

    mrs x1, CurrentEL
    lsr x20, x1, #2             /* record firmware/QEMU's initial EL */
    cmp x1, #8
    b.eq .Lfrom_el2
    cmp x1, #4
    b.eq .Lel1
    /* Give even an unsupported EL a private stack for its diagnostic. */
    msr spsel, #1
    adrp x2, __stack_top
    add x2, x2, :lo12:__stack_top
    mov sp, x2
    lsr x0, x1, #2
    bl aarch64_bad_el
    b .Lpark

.Lfrom_el2:
    /* Enter AArch64 EL1h with interrupts masked; leave the MMU disabled. */
    mov x2, #0x80000000
    msr hcr_el2, x2
    msr cptr_el2, xzr
    mov x2, #3
    msr cnthctl_el2, x2
    msr cntvoff_el2, xzr
    mrs x2, sctlr_el1
    bic x2, x2, #1
    bic x2, x2, #4
    bic x2, x2, #0x1000
    msr sctlr_el1, x2
    adrp x2, __stack_top
    add x2, x2, :lo12:__stack_top
    msr sp_el1, x2
    adr x2, .Lel1
    msr elr_el2, x2
    mov x2, #0x3c5              /* D/A/I/F masked, EL1h */
    msr spsr_el2, x2
    isb
    eret

.Lel1:
    msr spsel, #1
    adrp x1, __bss_start
    add x1, x1, :lo12:__bss_start
    adrp x2, __bss_end
    add x2, x2, :lo12:__bss_end
.Lzero_bss:
    cmp x1, x2
    b.hs .Lbss_done
    str xzr, [x1], #8
    b .Lzero_bss
.Lbss_done:
    adrp x1, __stack_top
    add x1, x1, :lo12:__stack_top
    mov sp, x1
    adrp x1, aarch64_vectors
    add x1, x1, :lo12:aarch64_vectors
    msr vbar_el1, x1
    isb
    mov x0, x19
    mov x1, x20
    bl aarch64_boot_main
.Lpark:
    wfe
    b .Lpark

/* A minimal diagnostic vector table; IRQ/FIQ remain masked. There is no
 * interrupt controller, exception recovery, scheduler or userspace yet. */
.balign 2048
aarch64_vectors:
.rept 16
    b .Lexception
    .balign 128
.endr
.Lexception:
    bl aarch64_exception
    b .Lpark

.section .bss.stack, "aw", %nobits
.balign 16
.global __stack_bottom
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:
