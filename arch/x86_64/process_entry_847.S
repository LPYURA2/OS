/* Candidate only. Same Microsoft x64 and up_context layout as T-P1.7.
 * One CPU, serialized; no SIMD, no IF, no nested dispatch. */
.text
.globl s847_enter
s847_enter:
    pushfq
    popq %rax
    testq $0x200,%rax
    jnz .Lbad847
    movq %cr3,%rax
    cmpq %r8,%rax
    jne .Lbad847
    cmpq %r8,%rdx
    je .Lbad847
    cmpq $0,s847_saved_sp(%rip)
    jne .Lbad847
    pushq %rbx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $40,%rsp
    movq %rsp,s847_saved_sp(%rip)
    movq %r8,s847_safe_root(%rip)
    movq %rdx,s847_child_root(%rip)
    xorq %rax,%rax
    movw %ss,%ax
    movw %ax,s847_kernel_ss(%rip)
#ifdef STYLEOS_TEST_FX_CONTEXT_849
    cmpq $0,s849_live(%rip)
    je .Lfxenter_done849
    cmpq $1,s849_live(%rip)
    jne .Lbad847
    fxrstor64 s849_user_restore(%rip)
.Lfxenter_done849:
#endif
    movq %rdx,%cr3
    movq %rcx,%r11
    pushq 280(%r11)
    pushq 248(%r11)
    pushq 264(%r11)
    pushq 272(%r11)
    pushq 256(%r11)
    movq 0(%r11),%rax
    movq 8(%r11),%rbx
    movq 16(%r11),%rcx
    movq 24(%r11),%rdx
    movq 32(%r11),%rsi
    movq 40(%r11),%rdi
    movq 48(%r11),%rbp
    movq 56(%r11),%r8
    movq 64(%r11),%r9
    movq 72(%r11),%r10
    movq 88(%r11),%r12
    movq 96(%r11),%r13
    movq 104(%r11),%r14
    movq 112(%r11),%r15
    movq 80(%r11),%r11
    iretq
.globl s847_pf_entry
s847_pf_entry:
    pushq $14
    jmp .Ltrap847
.globl s847_call_entry
s847_call_entry:
    pushq $0
    pushq $128
#ifdef STYLEOS_TEST_USER_PREEMPT_848
    jmp .Ltrap847
.globl s848_irq_entry
s848_irq_entry:
    pushq $0
    pushq $0x50
    jmp .Ltrap847
#endif
.Ltrap847:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    cld
    movq %rsp,%rbx
    /* Reject privileged origin before consuming user-only frame fields. */
    movq 144(%rsp),%rax
    andq $3,%rax
    cmpq $3,%rax
    jne .Lbad847
    cmpq $0,s847_saved_sp(%rip)
    je .Lbad847
    movq %cr3,%r8
    cmpq s847_child_root(%rip),%r8
    jne .Lbad847
#ifdef STYLEOS_TEST_FX_CONTEXT_849
    cmpq $0,s849_live(%rip)
    je .Lfxtrap_done849
    cmpq $1,s849_live(%rip)
    jne .Lbad847
    fxsave64 s849_user_capture(%rip)
    fxrstor64 s849_kernel_image(%rip)
    movq $2,s849_live(%rip)
.Lfxtrap_done849:
#endif
    movq %rsp,%rcx
    movq %cr2,%rdx
    andq $-16,%rsp
    subq $32,%rsp
    call s847_dispatch_raw
    cmpl $1,%eax
    je .Lreturn847
    testl %eax,%eax
    jne .Lbad847
    movq s847_safe_root(%rip),%rax
    movq %rax,%cr3
    movw s847_kernel_ss(%rip),%ax
    movw %ax,%ss
    movq s847_saved_sp(%rip),%rsp
    movq $0,s847_saved_sp(%rip)
    movq $0,s847_child_root(%rip)
    movq $0,s847_safe_root(%rip)
    addq $40,%rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rbx
    ret
.Lreturn847:
    movq %cr3,%rax
    cmpq s847_child_root(%rip),%rax
    jne .Lbad847
    movq %rbx,%rsp
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16,%rsp
    iretq
.Lbad847:
    cli
    movl $0x26,%eax
    outl %eax,$0xf4
.Lhalt847:
    hlt
    jmp .Lhalt847
.data
.p2align 3
s847_saved_sp: .quad 0
s847_safe_root: .quad 0
s847_child_root: .quad 0
s847_kernel_ss: .word 0

/* Relocation-free fixtures; copied to separate leased RX pages. */
.text
.globl s847_bad_entry
s847_bad_entry:
    jmp .Lbad847
.macro prefix847 tag
    movabs $0x12000000,%rbx
    movq $8,%rcx
    movq $3,%rax
    int $0x80
.globl s847_\tag\()_ok
s847_\tag\()_ok:
    testq %rax,%rax
    jnz .Luser_bad\tag
    movabs $0x400000003000,%rbx
    movq $8,%rcx
    movq $3,%rax
    int $0x80
.globl s847_\tag\()_deny
s847_\tag\()_deny:
    cmpq $3,%rax
    jne .Luser_bad\tag
    movabs $0x12000000,%rdi
    movq $0x847000,%r12
    addq $\tag,%r12
    movq %r12,(%rdi)
    pushq %r12
    movq $1,%rax
    int $0x80
.globl s847_\tag\()_yield
s847_\tag\()_yield:
    popq %r13
    cmpq %r12,%r13
    jne .Luser_bad\tag
    cmpq %r12,(%rdi)
    jne .Luser_bad\tag
.endm
.globl s847_0_start,s847_0_end,s847_0_fault
s847_0_start:
    prefix847 0
    movabs $0x400000003000,%rbx
s847_0_fault:
    movq (%rbx),%rax
.Luser_bad0:
    ud2
s847_0_end:
.globl s847_1_start,s847_1_end,s847_1_exit
s847_1_start:
    prefix847 1
    movq $0x847101,(%rdi)
    movq $2,%rax
    int $0x80
s847_1_exit:
.Luser_bad1:
    ud2
s847_1_end:

#ifdef STYLEOS_TEST_USER_PREEMPT_848
/* No voluntary yield or GPR changes in either busy loop. */
.text
.globl s848_a_start,s848_a_loop_end,s848_a_fault,s848_a_end
s848_a_start:
#ifdef STYLEOS_TEST_FX_CONTEXT_849
    movq %r14,%xmm14
    movq %r15,%xmm15
    fld1
    ldmxcsr .Lmxcsr_a849(%rip)
#endif
#ifdef STYLEOS_TEST_RUNTIME_MIXED_863
    movq $1,%rax
    int $0x80
.globl s848_a_yield
s848_a_yield:
    testq %rax,%rax
    jnz .Lmixed_bad_a
    movabs $0x8480000000000000,%rax
#endif
1:  incq (%rdi)
    cmpq $0,8(%rdi)
    je 1b
s848_a_loop_end:
    movabs $0x400000003000,%rax
s848_a_fault:
    movq (%rax),%rax
#ifdef STYLEOS_TEST_RUNTIME_MIXED_863
.Lmixed_bad_a:
#endif
    ud2
#ifdef STYLEOS_TEST_FX_CONTEXT_849
.Lmxcsr_a849: .long 0x1f80
#endif
s848_a_end:
.globl s848_b_start,s848_b_loop_end,s848_b_exit,s848_b_end
s848_b_start:
#ifdef STYLEOS_TEST_FX_CONTEXT_849
    movq %r14,%xmm14
    movq %r15,%xmm15
    fldz
    ldmxcsr .Lmxcsr_b849(%rip)
#endif
#ifdef STYLEOS_TEST_RUNTIME_MIXED_863
    movq $1,%rax
    int $0x80
.globl s848_b_yield
s848_b_yield:
    testq %rax,%rax
    jnz .Lmixed_bad_b
    movabs $0x8480000100000000,%rax
#endif
2:  incq (%rdi)
    cmpq $0,8(%rdi)
    je 2b
s848_b_loop_end:
    movq $2,%rax
    int $0x80
s848_b_exit:
#ifdef STYLEOS_TEST_RUNTIME_MIXED_863
.Lmixed_bad_b:
#endif
    ud2
#ifdef STYLEOS_TEST_FX_CONTEXT_849
.Lmxcsr_b849: .long 0x3f80
#endif
s848_b_end:
#endif
