/* Microsoft x64 ABI: RCX already contains boot_info. The firmware-provided
 * stack is replaced after ExitBootServices with a reserved image-owned stack. */
.text
.globl styleos_enter_kernel
.def styleos_enter_kernel; .scl 2; .type 32; .endef
styleos_enter_kernel:
    cli
    cld
    leaq styleos_stack_top(%rip), %rsp
    andq $-16, %rsp
    /* 32 bytes of MS x64 shadow space plus an 8-byte boot_info save and
     * alignment padding. Install our IDT only after switching to this stack. */
    subq $48, %rsp
    movq %rcx, 32(%rsp)
    call arch_exception_install
    movq 32(%rsp), %rcx
    call kernel_main
1:
    cli
    hlt
    jmp 1b

.bss
.p2align 4
.globl styleos_stack_bottom
.globl styleos_stack_top
styleos_stack_bottom:
    .skip 65536
styleos_stack_top:
