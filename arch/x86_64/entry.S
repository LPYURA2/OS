/* Microsoft x64 ABI: RCX already contains boot_info. The firmware-provided
 * stack is replaced after ExitBootServices with a reserved image-owned stack. */
.text
.globl styleos_enter_kernel
.def styleos_enter_kernel; .scl 2; .type 32; .endef
styleos_enter_kernel:
    cli
    leaq styleos_stack_top(%rip), %rsp
    andq $-16, %rsp
    subq $32, %rsp
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
