/* AAPCS64 x0 is the canonical, deliberately unmapped test VA. The exported
 * fault-site symbol names the actual read instruction, not a C prologue. */
.section .text.paging_fault, "ax"
.balign 4
.global arch_paging_fault
.type arch_paging_fault, %function
arch_paging_fault:
.global styleos_paging_fault
.type styleos_paging_fault, %function
styleos_paging_fault:
    ldr x1, [x0]
    ret
.size styleos_paging_fault, . - styleos_paging_fault
.size arch_paging_fault, . - arch_paging_fault
