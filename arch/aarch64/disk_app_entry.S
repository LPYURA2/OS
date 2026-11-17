/* T-P1.8B trusted hardware glue only. The EL0 program is read from disk into
 * admitted BSS; this object contains no built-in user program. */
.section .text.disk_app, "ax"
.balign 16
.global da_arm_enter
.type da_arm_enter, %function
da_arm_enter:
    /* x0=full initial integer context, x1=APP root, x2=trusted entry SP. */
    sub sp, sp, #96
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    adrp x3, styleos_disk_app_control
    add x3, x3, :lo12:styleos_disk_app_control
    add x3, x3, #12288
    mov x4, sp
    str x4, [x3]
    dsb sy
    msr ttbr0_el1, x1
    isb
    tlbi vmalle1
    dsb sy
    isb
    mov sp, x2
    mov x30, x0
    ldr x3, [x30, #248]
    msr sp_el0, x3
    ldr x3, [x30, #256]
    msr elr_el1, x3
    ldr x3, [x30, #264]
    msr spsr_el1, x3
    ldp x0, x1, [x30, #0]
    ldp x2, x3, [x30, #16]
    ldp x4, x5, [x30, #32]
    ldp x6, x7, [x30, #48]
    ldp x8, x9, [x30, #64]
    ldp x10, x11, [x30, #80]
    ldp x12, x13, [x30, #96]
    ldp x14, x15, [x30, #112]
    ldp x16, x17, [x30, #128]
    ldp x18, x19, [x30, #144]
    ldp x20, x21, [x30, #160]
    ldp x22, x23, [x30, #176]
    ldp x24, x25, [x30, #192]
    ldp x26, x27, [x30, #208]
    ldp x28, x29, [x30, #224]
    ldr x30, [x30, #240]
    eret
.global da_arm_enter_end
da_arm_enter_end:
.size da_arm_enter, . - da_arm_enter

.balign 16
.Luser_sync:
    /* Hardware selected trusted SP_EL1. Preserve every user GPR first. */
    sub sp, sp, #320
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    mrs x0, sp_el0
    str x0, [sp, #248]
    mrs x0, elr_el1
    str x0, [sp, #256]
    mrs x0, spsr_el1
    str x0, [sp, #264]
    mrs x0, esr_el1
    str x0, [sp, #272]
    mrs x0, far_el1
    str x0, [sp, #280]
    mrs x0, ttbr0_el1
    str x0, [sp, #288]
    mov x0, sp
    str x0, [sp, #296]
    mov x1, #8
    str x1, [sp, #304]
    str xzr, [sp, #312]
    bl da_arm_capture
    /* Capture installed SAFE. Restore the original dispatcher stack before
     * clearing its active context, then preserve ordinary AAPCS64 return. */
    adrp x0, styleos_disk_app_control
    add x0, x0, :lo12:styleos_disk_app_control
    add x0, x0, #12288
    ldr x0, [x0]
    mov sp, x0
    bl da_arm_finish
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #96
    mov x0, #0
    ret

.Lkernel_sync:
    mrs x0, esr_el1
    mrs x1, elr_el1
    mrs x2, spsr_el1
    mrs x3, far_el1
    bl early_exception_arm_sync
.Lterminal:
    wfe
    b .Lterminal
.Lunsupported:
    bl early_exception_arm_unsupported
    b .Lterminal
.macro KERNEL_SYNC_VECTOR
    b .Lkernel_sync
    .rept 31
    nop
    .endr
.endm
.macro UNSUPPORTED_DISKAPP_VECTOR slot
    mov x0, #\slot
    b .Lunsupported
    .rept 30
    nop
    .endr
.endm
.balign 2048
.global styleos_disk_app_vectors
.type styleos_disk_app_vectors, %object
styleos_disk_app_vectors:
    KERNEL_SYNC_VECTOR
    UNSUPPORTED_DISKAPP_VECTOR 1
    UNSUPPORTED_DISKAPP_VECTOR 2
    UNSUPPORTED_DISKAPP_VECTOR 3
    KERNEL_SYNC_VECTOR
    UNSUPPORTED_DISKAPP_VECTOR 5
    UNSUPPORTED_DISKAPP_VECTOR 6
    UNSUPPORTED_DISKAPP_VECTOR 7
    b .Luser_sync
    .rept 31
    nop
    .endr
    UNSUPPORTED_DISKAPP_VECTOR 9
    UNSUPPORTED_DISKAPP_VECTOR 10
    UNSUPPORTED_DISKAPP_VECTOR 11
    KERNEL_SYNC_VECTOR
    UNSUPPORTED_DISKAPP_VECTOR 13
    UNSUPPORTED_DISKAPP_VECTOR 14
    UNSUPPORTED_DISKAPP_VECTOR 15
.global styleos_disk_app_vectors_end
styleos_disk_app_vectors_end:
.size styleos_disk_app_vectors, . - styleos_disk_app_vectors
.if (. - styleos_disk_app_vectors) != 2048
.error "T-P1.8B vectors must occupy exactly 2048 bytes"
.endif
