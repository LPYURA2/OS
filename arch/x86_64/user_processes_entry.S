/* T-P1.7 only. Full exception-observed integer state, not the T-P1.6 ABI.
 * Opaque admitted control scratch begins at byte12288. Its first words are
 * dispatcherSP, context*, event*, safeCR3, selectedCR3 and running slot.
 * Microsoft x64 call ABI applies only on the trusted dispatcher/C sides. */
.text
.p2align 4
.globl up_x86_enter
.def up_x86_enter; .scl 2; .type 32; .endef
up_x86_enter:
    pushq %rbx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $40, %rsp
    movq %rsp, styleos_user_control+12288(%rip)
    movq %rcx, styleos_user_control+12296(%rip)
    movq %rdx, styleos_user_control+12304(%rip)
    movq %r8, styleos_user_control+12320(%rip)
    movq %r8, %cr3
    /* Build all five IRET words on the trusted dispatcher stack. CPL3 will
     * subsequently enter through TSS.RSP0, independent of this user SP. */
    movq %rcx, %r11
    pushq 280(%r11)
    pushq 248(%r11)
    pushq 264(%r11)
    pushq 272(%r11)
    pushq 256(%r11)
    movq 0(%r11), %rax
    movq 8(%r11), %rbx
    movq 16(%r11), %rcx
    movq 24(%r11), %rdx
    movq 32(%r11), %rsi
    movq 40(%r11), %rdi
    movq 48(%r11), %rbp
    movq 56(%r11), %r8
    movq 64(%r11), %r9
    movq 72(%r11), %r10
    movq 88(%r11), %r12
    movq 96(%r11), %r13
    movq 104(%r11), %r14
    movq 112(%r11), %r15
    movq 80(%r11), %r11
.globl styleos_user_x86_iret
styleos_user_x86_iret:
    iretq
.globl up_x86_enter_end
up_x86_enter_end:

.p2align 4
.globl styleos_user_x86_pf
.def styleos_user_x86_pf; .scl 2; .type 32; .endef
styleos_user_x86_pf:
    /* Hardware already pushed error, RIP, CS, RFLAGS, userRSP and userSS. */
    pushq $14
    jmp styleos_user_x86_trap_common
.globl styleos_user_x86_call
.def styleos_user_x86_call; .scl 2; .type 32; .endef
styleos_user_x86_call:
    pushq $0
    pushq $128
    jmp styleos_user_x86_trap_common

.p2align 4
.globl styleos_user_x86_trap_common
.def styleos_user_x86_trap_common; .scl 2; .type 32; .endef
styleos_user_x86_trap_common:
    /* Preserve every non-SP integer GPR before using even volatile ones.
     * Raw176-byte frame:15GPRs,vector,error,RIP,CS,RFLAGS,userRSP,userSS.
     * Privileged-origin entries have no last two words; C checks CS first. */
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
    movq %rsp, %rcx
    movq %cr2, %rdx
    movq %cr3, %r8
    /* Both accepted user gates leave this frame16-aligned. Align again for
     * terminal diagnostics on an unexpected privileged-origin fault. */
    andq $-16, %rsp
    subq $32, %rsp
    call up_x86_trap
    /* Discard the complete user entry stack and its context only after the
     * safe root is active. Never IRET to a faulted user's PC from this path. */
    movq styleos_user_control+12312(%rip), %rax
    movq %rax, %cr3
    movw $16, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    movq styleos_user_control+12288(%rip), %rsp
.globl styleos_user_x86_dispatch_resume
styleos_user_x86_dispatch_resume:
    addq $40, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rbx
    ret
.globl styleos_user_x86_trap_common_end
styleos_user_x86_trap_common_end:

/* Only this new profile installs an image-owned GDT/TSS. RDX is IDTR,
 * RCX transitionalGDTR. A retained old-CS alias makes the old IDT valid until
 * LIDT; C narrows the GDT only afterwards. IF stays clear throughout. There
 * are no memory operations using a changed selector outside admitted image
 * storage. Old terminal ISR targets remain image mapped. */
.p2align 4
.globl up_x86_descriptors
.def up_x86_descriptors; .scl 2; .type 32; .endef
up_x86_descriptors:
    lgdt (%rcx)
    pushq $8
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:  movw $16, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $40, %ax
    ltr %ax
    lidt (%rdx)
    ret
.globl up_x86_descriptors_end
up_x86_descriptors_end:

/* File-backed whole pages: no relocations, C calls, FP/SIMD or supervisor
 * instructions occur in these embedded integer-only user programs. Only
 * the intended root maps its own page as user RX; padding stays inside it. */
.text
.p2align 12
.globl styleos_user_code_a
.globl styleos_user_a_start
styleos_user_code_a:
styleos_user_a_start:
    subq $16, %rsp
    movabsq $0xa11ce55a12345678, %r12
    movq %r12, (%rsp)
    movq $0xa11c, %rbx
#ifdef STYLEOS_TEST_USER_SYSCALL
    /* Diagnostic INT80 only: RDX = CPL3 pointer, RCX = length. */
#ifdef STYLEOS_TEST_USER_SYSCALL_DENY
    movabsq $0x12010000, %rdx
#else
    movq %rsp, %rdx
#endif
    movq $8, %rcx
    movq $3, %rax
#else
    movq $1, %rax
#endif
.globl styleos_user_a_yield
styleos_user_a_yield:
    int $0x80
.globl styleos_user_a_yield_resume
styleos_user_a_yield_resume:
    movabsq $0x12010000, %rdi
.globl styleos_user_a_fault
styleos_user_a_fault:
    movq (%rdi), %rax
    /* Reaching here would mean isolation failed; force invalid operation. */
    movq $3, %rax
    int $0x80
2:  jmp 2b
    .fill 4096-(.-styleos_user_code_a),1,0x90
.globl styleos_user_code_a_end
styleos_user_code_a_end:

.p2align 12
.globl styleos_user_code_b
.globl styleos_user_b_start
styleos_user_code_b:
styleos_user_b_start:
    subq $16, %rsp
    movabsq $0x123456789abcdef0, %r12
    movq %r12, (%rsp)
    movq $0xb22c, %rbx
    movabsq $0x55aa001122334455, %r10
    movabsq $0x12010000, %rdi
    movq %r12, (%rdi)
    movq $1, %rax
.globl styleos_user_b_yield
styleos_user_b_yield:
    int $0x80
.globl styleos_user_b_yield_resume
styleos_user_b_yield_resume:
    movabsq $0x55aa001122334455, %rax
    cmpq %rax, %r10
    jne 3f
    cmpq %r12, (%rsp)
    jne 3f
    cmpq %r12, (%rdi)
    jne 3f
    movabsq $0xfedcba9876543210, %rax
    movq %rax, (%rdi)
    movq $2, %rax
.globl styleos_user_b_exit
styleos_user_b_exit:
    int $0x80
.globl styleos_user_b_exit_resume
styleos_user_b_exit_resume:
3:  movq $3, %rax
    int $0x80
4:  jmp 4b
    .fill 4096-(.-styleos_user_code_b),1,0x90
.globl styleos_user_code_b_end
styleos_user_code_b_end:
