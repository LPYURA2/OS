/* Every path is terminal. No register restoration, IRET or stack-failure
 * recovery is promised. Interrupt gates keep IF clear; NMI/MC/IRQ vectors
 * are tagged by their vector and classified as unsupported by common C. */
.text

.macro X86_ISR number
.globl styleos_isr_\number
.def styleos_isr_\number; .scl 2; .type 32; .endef
styleos_isr_\number:
    /* Hardware error-code exceptions already have an error at old RSP.
     * All other vectors get padding, explicitly marked as NOT an error code. */
    .if (\number == 8) || (\number == 10) || (\number == 11) || (\number == 12) || (\number == 13) || (\number == 14) || (\number == 17) || (\number == 21) || (\number == 29) || (\number == 30)
        pushq $1
    .else
        pushq $0
        pushq $0
    .endif
    pushq $\number
    jmp styleos_exception_common
.endm

.irp vector,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    X86_ISR \vector
.endr

/* Normalized entry frame (all qwords):
 * RSP+0 vector; +8 has_error; +16 error/padding; +24 saved RIP;
 * +32 saved CS; +40 saved RFLAGS. No saved address is dereferenced.
 * Capture the hardware values before realigning the stack for C. */
styleos_exception_common:
    cli
    movq 24(%rsp), %rdx
    movq 16(%rsp), %r9
    movq 8(%rsp), %r8
    movq 0(%rsp), %rcx
    xorl %eax, %eax
    cmpq $14, %rcx
    jne 1f
    movq %cr2, %rax
1:
    cld
    /* MS x64 ABI: RCX/RDX/R8/R9, argument5 at [caller RSP+32].
     * Alignment is independent of the interrupted RSP and hardware frame.
     * 48 bytes provide 32 shadow + 8 fifth argument + 8 alignment padding. */
    andq $-16, %rsp
    subq $48, %rsp
    movq %rax, 32(%rsp)
    call early_exception_x86
2:
    cli
    hlt
    jmp 2b

.section .rdata,"dr"
.p2align 3
.globl styleos_isr_table
styleos_isr_table:
.irp vector,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    .quad styleos_isr_\vector
.endr

#ifdef STYLEOS_TEST_EXCEPTION
.text
.globl styleos_test_fault
.def styleos_test_fault; .scl 2; .type 32; .endef
styleos_test_fault:
    ud2
3:
    cli
    hlt
    jmp 3b
#endif
