/* T-P1.7 only: EL0 programs, full exception capture, and trusted dispatcher
 * return. None of this object is linked into any older profile. */
.section .text.user_processes, "ax"
.balign 16
.global up_arm_enter
.type up_arm_enter, %function
up_arm_enter:
    /* x0=validated user context, x1=process root, x2=entry-stack top.
     * Preserve the caller's original kernel stack and AAPCS64 registers. */
    sub sp, sp, #96
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    adrp x3, styleos_user_control
    add x3, x3, :lo12:styleos_user_control
    add x3, x3, #12288
    mov x4, sp
    str x4, [x3, #0]             /* ARM scratch.dispatcher_sp */
    dsb sy
    msr ttbr0_el1, x1
    isb
    tlbi vmalle1
    dsb sy
    isb
    mov sp, x2                  /* SP_EL1 independent from user SP_EL0 */
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
.global up_arm_enter_end
up_arm_enter_end:
.size up_arm_enter, . - up_arm_enter

.balign 16
.Luser_sync:
    /* No user GPR is clobbered before all X0-X30 are saved. The hardware
     * selects SP_EL1 for this lower-EL exception, not untrusted SP_EL0. */
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
    bl up_arm_capture
    /* capture has installed and checked SAFE before a failed process frame
     * can leave use. Do not ERET to either the fault PC or any user program. */
    adrp x0, styleos_user_control
    add x0, x0, :lo12:styleos_user_control
    add x0, x0, #12288
    ldr x0, [x0, #0]
    mov sp, x0
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
.macro UNSUPPORTED_USER_VECTOR slot
    mov x0, #\slot
    b .Lunsupported
    .rept 30
    nop
    .endr
.endm
.balign 2048
.global styleos_user_vectors
.type styleos_user_vectors, %object
styleos_user_vectors:
    KERNEL_SYNC_VECTOR
    UNSUPPORTED_USER_VECTOR 1
    UNSUPPORTED_USER_VECTOR 2
    UNSUPPORTED_USER_VECTOR 3
    KERNEL_SYNC_VECTOR
    UNSUPPORTED_USER_VECTOR 5
    UNSUPPORTED_USER_VECTOR 6
    UNSUPPORTED_USER_VECTOR 7
    b .Luser_sync
    .rept 31
    nop
    .endr
    UNSUPPORTED_USER_VECTOR 9
    UNSUPPORTED_USER_VECTOR 10
    UNSUPPORTED_USER_VECTOR 11
    KERNEL_SYNC_VECTOR
    UNSUPPORTED_USER_VECTOR 13
    UNSUPPORTED_USER_VECTOR 14
    UNSUPPORTED_USER_VECTOR 15
.global styleos_user_vectors_end
styleos_user_vectors_end:
.size styleos_user_vectors, . - styleos_user_vectors
.if (. - styleos_user_vectors) != 2048
.error "T-P1.7 vectors must occupy exactly2048bytes"
.endif

.macro LOAD64 reg, a, b, c, d
    movz \reg, #\a
    movk \reg, #\b, lsl #16
    movk \reg, #\c, lsl #32
    movk \reg, #\d, lsl #48
.endm

.balign 4096
.global styleos_user_code_a
.global styleos_user_a_start
.type styleos_user_code_a, %object
styleos_user_code_a:
styleos_user_a_start:
    mov x19, #0xa11c
    LOAD64 x20, 0x5678, 0x1234, 0xe55a, 0xa11c
    sub sp, sp, #16
    str x20, [sp]
    mov x0, #1
.global styleos_user_a_yield
styleos_user_a_yield:
    svc #1
.global styleos_user_a_yield_resume
styleos_user_a_yield_resume:
    ldr x2, [sp]
    cmp x2, x20
    b.ne .La_bad
    mov x2, #0xa11c
    cmp x19, x2
    b.ne .La_bad
    movz x0, #0x1201, lsl #16
.global styleos_user_a_fault
styleos_user_a_fault:
    ldr x1, [x0]
.La_bad:
    mov x0, #3
    svc #3
    b .La_bad
.global styleos_user_a_end
styleos_user_a_end:
    .rept (4096 - (. - styleos_user_code_a)) / 4
    nop
    .endr
.global styleos_user_code_a_end
styleos_user_code_a_end:
.size styleos_user_code_a, . - styleos_user_code_a

.balign 4096
.global styleos_user_code_b
.global styleos_user_b_start
.type styleos_user_code_b, %object
styleos_user_code_b:
styleos_user_b_start:
    mov x19, #0xb22c
    LOAD64 x20, 0xdef0, 0x9abc, 0x5678, 0x1234
    LOAD64 x9, 0x4455, 0x2233, 0x0011, 0x55aa
    sub sp, sp, #16
    str x20, [sp]
    movz x21, #0x1201, lsl #16
    str x20, [x21]
    mov x0, #1
.global styleos_user_b_yield
styleos_user_b_yield:
    svc #1
.global styleos_user_b_yield_resume
styleos_user_b_yield_resume:
    LOAD64 x2, 0x4455, 0x2233, 0x0011, 0x55aa
    cmp x9, x2
    b.ne .Lb_bad
    ldr x2, [sp]
    cmp x2, x20
    b.ne .Lb_bad
    mov x2, #0xb22c
    cmp x19, x2
    b.ne .Lb_bad
    movz x2, #0x1201, lsl #16
    cmp x21, x2
    b.ne .Lb_bad
    ldr x2, [x21]
    cmp x2, x20
    b.ne .Lb_bad
    LOAD64 x2, 0x3210, 0x7654, 0xba98, 0xfedc
    str x2, [x21]
    mov x0, #2
.global styleos_user_b_exit
styleos_user_b_exit:
    svc #2
.Lb_bad:
    mov x0, #3
    svc #3
    b .Lb_bad
.global styleos_user_b_end
styleos_user_b_end:
    .rept (4096 - (. - styleos_user_code_b)) / 4
    nop
    .endr
.global styleos_user_code_b_end
styleos_user_code_b_end:
.size styleos_user_code_b, . - styleos_user_code_b
