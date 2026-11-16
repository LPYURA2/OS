/* Experimental integer-only AAPCS64 call-boundary context.  SIMD/FP and
 * caller-clobbered registers are outside this deliberately narrow ABI. */
.text
.p2align 4
.globl tasks_arch_switch
.type tasks_arch_switch, %function
tasks_arch_switch:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x9, sp
    str x9, [x0, #96]
    str x30, [x0, #104]
    ldr x16, [x1, #104]
    ldr x9, [x1, #96]
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    mov sp, x9
    br x16
.globl tasks_arch_switch_end
tasks_arch_switch_end:
.size tasks_arch_switch, .-tasks_arch_switch

.p2align 4
.globl tasks_arch_dispatch
.type tasks_arch_dispatch, %function
tasks_arch_dispatch:
    stp x29, x30, [sp, #-16]!
    bl tasks_arch_switch
.globl styleos_tasks_dispatch_resume
styleos_tasks_dispatch_resume:
    ldp x29, x30, [sp], #16
    ret
.globl tasks_arch_dispatch_end
tasks_arch_dispatch_end:
.size tasks_arch_dispatch, .-tasks_arch_dispatch

.p2align 4
.globl tasks_arch_probe
.type tasks_arch_probe, %function
tasks_arch_probe:
    sub sp, sp, #128
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    stp x0, x1, [sp, #96]
    str x2, [sp, #112]
    mov x9, #0x1122000000000000
    add x9, x9, x2
    mov x10, #0x100000000
    mov x19, x9
    add x9, x9, x10
    mov x20, x9
    add x9, x9, x10
    mov x21, x9
    add x9, x9, x10
    mov x22, x9
    add x9, x9, x10
    mov x23, x9
    add x9, x9, x10
    mov x24, x9
    add x9, x9, x10
    mov x25, x9
    add x9, x9, x10
    mov x26, x9
    add x9, x9, x10
    mov x27, x9
    add x9, x9, x10
    mov x28, x9
    add x9, x9, x10
    mov x29, x9
    ldp x0, x1, [sp, #96]
    bl tasks_arch_switch
.globl styleos_tasks_probe_resume
styleos_tasks_probe_resume:
    mov x0, #0
    mov x9, #0x1122000000000000
    ldr x11, [sp, #112]
    add x9, x9, x11
    mov x10, #0x100000000
    cmp x19, x9
    b.ne 1f
    orr x0, x0, #1
1:  add x9, x9, x10
    cmp x20, x9
    b.ne 2f
    orr x0, x0, #2
2:  add x9, x9, x10
    cmp x21, x9
    b.ne 3f
    orr x0, x0, #4
3:  add x9, x9, x10
    cmp x22, x9
    b.ne 4f
    orr x0, x0, #8
4:  add x9, x9, x10
    cmp x23, x9
    b.ne 5f
    orr x0, x0, #16
5:  add x9, x9, x10
    cmp x24, x9
    b.ne 6f
    orr x0, x0, #32
6:  add x9, x9, x10
    cmp x25, x9
    b.ne 7f
    orr x0, x0, #64
7:  add x9, x9, x10
    cmp x26, x9
    b.ne 8f
    orr x0, x0, #128
8:  add x9, x9, x10
    cmp x27, x9
    b.ne 9f
    orr x0, x0, #256
9:  add x9, x9, x10
    cmp x28, x9
    b.ne 10f
    orr x0, x0, #512
10: add x9, x9, x10
    cmp x29, x9
    b.ne 11f
    orr x0, x0, #1024
11: ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #128
    ret
.globl tasks_arch_probe_end
tasks_arch_probe_end:
.size tasks_arch_probe, .-tasks_arch_probe

.p2align 4
.globl tasks_arch_finish
.type tasks_arch_finish, %function
tasks_arch_finish:
    sub sp, sp, #16
    stp x29, x30, [sp]
    bl tasks_arch_switch
.globl styleos_tasks_exit_resume
styleos_tasks_exit_resume:
    bl tasks_completed_resumed
12: wfi
    b 12b
.globl tasks_arch_finish_end
tasks_arch_finish_end:
.size tasks_arch_finish, .-tasks_arch_finish

.p2align 4
.globl styleos_tasks_trap_return
.type styleos_tasks_trap_return, %function
styleos_tasks_trap_return:
    bl tasks_completed_resumed
13: wfi
    b 13b
.globl styleos_tasks_trap_return_end
styleos_tasks_trap_return_end:
.size styleos_tasks_trap_return, .-styleos_tasks_trap_return
.section .note.GNU-stack,"",%progbits
