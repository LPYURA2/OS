/* Terminal-only Microsoft x64 ABI IRQ entry. */
.text
.globl s843_irq_entry
s843_irq_entry:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    cld
    movq %rsp,%rbx
    movq %rsp,%rcx
    andq $-16,%rsp
    subq $32,%rsp
    call s843_irq_dispatch
    movq %rbx,%rsp
    testb %al,%al
    jz .Lfail843
.Lrestore843:
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    iretq
.globl s843_enter_first
s843_enter_first:
    cli
    movq %rcx,%rsp
    jmp .Lrestore843
.Lfail843:
    cli
    movl $0x23,%eax
    outl %eax,$0xf4
.Lhalt843:
    hlt
    jmp .Lhalt843
