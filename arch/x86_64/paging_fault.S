/* Dedicated T-P1.4 fault image. Microsoft x64 first argument is RCX.
 * The exported fault site names the actual supervisor read, not a wrapper. */
.text
.globl arch_paging_fault
.def arch_paging_fault; .scl 2; .type 32; .endef
arch_paging_fault:
.globl styleos_paging_fault
.def styleos_paging_fault; .scl 2; .type 32; .endef
styleos_paging_fault:
    movq (%rcx), %rax
    retq
