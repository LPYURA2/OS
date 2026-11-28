.section .text.entry,"ax",@progbits
.global _start
_start:
    and $-16,%rsp
    call explorer_main
    mov $2,%eax
    int $0x80
    ud2
.section .note.GNU-stack,"",@progbits
