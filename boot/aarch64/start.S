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
    /* No own exception coverage is claimed before the UART, stack and VBAR
     * are ready. x19/x20 survive the AAPCS64 console call. */
    bl arch_console_init
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

/* Every entry has its architectural 128-byte size. For synchronous entries,
 * capture syndrome, saved PC and saved PSTATE before any C code or other
 * calls; FAR is captured afterwards and its validity is decoded from ESR.
 * Terminal handlers intentionally do not preserve or restore interrupted GPRs.
 * IRQ/FIQ/SError are unsupported: never present stale ESR as a fresh syndrome. */
.macro SYNC_VECTOR
    mrs x0, esr_el1
    mrs x1, elr_el1
    mrs x2, spsr_el1
    mrs x3, far_el1
    b .Lsync_exception
    .space 108
.endm

.macro UNSUPPORTED_VECTOR slot
    mov x0, #\slot
    b .Lunsupported_exception
    .space 120
.endm

.balign 2048
.global aarch64_vectors
.type aarch64_vectors, %object
aarch64_vectors:
    /* Current EL using SP_EL0. */
    SYNC_VECTOR
    UNSUPPORTED_VECTOR 1
    UNSUPPORTED_VECTOR 2
    UNSUPPORTED_VECTOR 3
    /* Current EL using SP_EL1: the normal StyleOS early-kernel path. */
    SYNC_VECTOR
    UNSUPPORTED_VECTOR 5
    UNSUPPORTED_VECTOR 6
    UNSUPPORTED_VECTOR 7
    /* Lower EL, AArch64. No EL0 runtime is enabled by this stage. */
    SYNC_VECTOR
    UNSUPPORTED_VECTOR 9
    UNSUPPORTED_VECTOR 10
    UNSUPPORTED_VECTOR 11
    /* Lower EL, AArch32. No AArch32 runtime is enabled by this stage. */
    SYNC_VECTOR
    UNSUPPORTED_VECTOR 13
    UNSUPPORTED_VECTOR 14
    UNSUPPORTED_VECTOR 15
.size aarch64_vectors, . - aarch64_vectors
.if (. - aarch64_vectors) != 2048
.error "AArch64 EL1 vector table must be exactly 2048 bytes"
.endif

.Lsync_exception:
    bl early_exception_arm_sync
    b .Lpark
.Lunsupported_exception:
    bl early_exception_arm_unsupported
    b .Lpark

#ifdef STYLEOS_TEST_EXCEPTION
/* Only --test-exception images contain this managed fault. The common kernel
 * calls it after successful handoff and MEMMAP PASS, never during normal boot.
 * The exported PC label names the BRK instruction itself, not a C wrapper. */
.section .text.test_exception, "ax"
.balign 4
.global arch_test_exception
.type arch_test_exception, %function
arch_test_exception:
.global styleos_test_fault
styleos_test_fault:
    brk #0x120
    ret
.size arch_test_exception, . - arch_test_exception
#endif

.section .bss.stack, "aw", %nobits
.balign 16
.global __stack_bottom
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:
