/* Ordinary transition IDT unexpected vector: no debug-port exit, no
 * register modification, no return to unverified state. */
.text
.globl irq807_unexpected
irq807_unexpected:
    cli
1:  hlt
    jmp 1b
