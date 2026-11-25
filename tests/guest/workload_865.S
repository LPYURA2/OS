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
#ifdef STYLEOS_TEST_FOUR_GUEST_864
.macro extra848 name, tag, op, result
.globl s848_\name\()_start,s848_\name\()_yield,s848_\name\()_loop_end,s848_\name\()_fault,s848_\name\()_end
s848_\name\()_start:
    movq %r14,%xmm14
    movq %r15,%xmm15
    fldz
    ldmxcsr .Lmxcsr\name(%rip)
    movq $3,%rax
    int $0x80
s848_\name\()_yield:
    cmpq $\result,%rax
    jnz .Lbad\name
    movabs $(\tag+1),%rbx
    movabs $(\tag+2),%rcx
    movabs $\tag,%rax
1:  incq (%rdi)
    cmpq $0,8(%rdi)
    je 1b
s848_\name\()_loop_end:
s848_\name\()_fault:
    \op
.Lbad\name:
    ud2
.Lmxcsr\name: .long 0x3f80
s848_\name\()_end:
.endm
extra848 c,0x8480000200000000,ud2,0
extra848 d,0x8480000300000000,cli,3
#endif
#endif
