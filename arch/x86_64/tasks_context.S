/* Experimental integer-only Microsoft x64 call-boundary context.  RFLAGS,
 * volatile registers and FP/SIMD state are deliberately outside this ABI. */
.text
.p2align 4
.globl tasks_arch_switch
.def tasks_arch_switch; .scl 2; .type 32; .endef
tasks_arch_switch:
    movq %rbx, 0(%rcx)
    movq %rbp, 8(%rcx)
    movq %rdi, 16(%rcx)
    movq %rsi, 24(%rcx)
    movq %r12, 32(%rcx)
    movq %r13, 40(%rcx)
    movq %r14, 48(%rcx)
    movq %r15, 56(%rcx)
    xorl %eax, %eax
    movq %rax, 64(%rcx)
    movq %rax, 72(%rcx)
    movq %rax, 80(%rcx)
    movq %rax, 88(%rcx)
    leaq 8(%rsp), %rax
    movq %rax, 96(%rcx)
    movq (%rsp), %rax
    movq %rax, 104(%rcx)
    movq 104(%rdx), %rax
    movq 96(%rdx), %r8
    movq 0(%rdx), %rbx
    movq 8(%rdx), %rbp
    movq 16(%rdx), %rdi
    movq 24(%rdx), %rsi
    movq 32(%rdx), %r12
    movq 40(%rdx), %r13
    movq 48(%rdx), %r14
    movq 56(%rdx), %r15
    movq %r8, %rsp
    jmp *%rax
.globl tasks_arch_switch_end
tasks_arch_switch_end:

.p2align 4
.globl tasks_arch_dispatch
.def tasks_arch_dispatch; .scl 2; .type 32; .endef
tasks_arch_dispatch:
    subq $40, %rsp
    call tasks_arch_switch
.globl styleos_tasks_dispatch_resume
styleos_tasks_dispatch_resume:
    addq $40, %rsp
    ret
.globl tasks_arch_dispatch_end
tasks_arch_dispatch_end:

/* Save the real caller's registers before installing the task-specific test
 * values.  The 72-byte local frame provides shadow space and call alignment;
 * saved arguments are outside the callee's 32-byte shadow area. */
.p2align 4
.globl tasks_arch_probe
.def tasks_arch_probe; .scl 2; .type 32; .endef
tasks_arch_probe:
    pushq %rbx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $72, %rsp
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movabsq $0x1122000000000000, %rax
    addq %r8, %rax
    movabsq $0x100000000, %r9
    movq %rax, %rbx
    addq %r9, %rax
    movq %rax, %rbp
    addq %r9, %rax
    movq %rax, %rdi
    addq %r9, %rax
    movq %rax, %rsi
    addq %r9, %rax
    movq %rax, %r12
    addq %r9, %rax
    movq %rax, %r13
    addq %r9, %rax
    movq %rax, %r14
    addq %r9, %rax
    movq %rax, %r15
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    call tasks_arch_switch
.globl styleos_tasks_probe_resume
styleos_tasks_probe_resume:
    xorl %eax, %eax
    movabsq $0x1122000000000000, %r10
    addq 48(%rsp), %r10
    movabsq $0x100000000, %r9
    cmpq %r10, %rbx
    jne 1f
    orq $1, %rax
1:  addq %r9, %r10
    cmpq %r10, %rbp
    jne 2f
    orq $2, %rax
2:  addq %r9, %r10
    cmpq %r10, %rdi
    jne 3f
    orq $4, %rax
3:  addq %r9, %r10
    cmpq %r10, %rsi
    jne 4f
    orq $8, %rax
4:  addq %r9, %r10
    cmpq %r10, %r12
    jne 5f
    orq $16, %rax
5:  addq %r9, %r10
    cmpq %r10, %r13
    jne 6f
    orq $32, %rax
6:  addq %r9, %r10
    cmpq %r10, %r14
    jne 7f
    orq $64, %rax
7:  addq %r9, %r10
    cmpq %r10, %r15
    jne 8f
    orq $128, %rax
8:  addq $72, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rbx
    ret
.globl tasks_arch_probe_end
tasks_arch_probe_end:

.p2align 4
.globl tasks_arch_finish
.def tasks_arch_finish; .scl 2; .type 32; .endef
tasks_arch_finish:
    subq $40, %rsp
    call tasks_arch_switch
.globl styleos_tasks_exit_resume
styleos_tasks_exit_resume:
    /* A completed context must never be selected.  If it is, report the
     * contract violation through C using the still-valid shadow frame. */
    call tasks_completed_resumed
9:  cli
    hlt
    jmp 9b
.globl tasks_arch_finish_end
tasks_arch_finish_end:

.p2align 4
.globl styleos_tasks_trap_return
.def styleos_tasks_trap_return; .scl 2; .type 32; .endef
styleos_tasks_trap_return:
    /* Accidental RET from the initial bootstrap consumes its synthetic
     * return slot, leaving aligned RSP; reserve the required shadow space. */
    subq $32, %rsp
    call tasks_completed_resumed
10: cli
    hlt
    jmp 10b
.globl styleos_tasks_trap_return_end
styleos_tasks_trap_return_end:
