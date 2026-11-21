/* Minimal integer-only native user program; linked as ELF64 ET_EXEC,
 * packed into a .tea, then extracted into an image-owned user RX page.
 * This object is never linked into StyleOS's PE image. */
.text
.globl tea_768_start
.globl tea_768_yield
.globl tea_768_resume
.globl tea_768_exit
.globl tea_768_exit_resume
tea_768_start:
    nop  /* distinguish loaded ELF from pre-existing inline B program */
    subq $16, %rsp
    movabsq $0x123456789abcdef0, %r12
    movq %r12, (%rsp)
    movq $0xb22c, %rbx
    movabsq $0x55aa001122334455, %r10
    movabsq $0x12010000, %rdi
    movq %r12, (%rdi)
    movq $1, %rax
tea_768_yield:
    int $0x80
tea_768_resume:
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
tea_768_exit:
    int $0x80
tea_768_exit_resume:
3:  movq $3, %rax
    int $0x80
4:  jmp 4b
