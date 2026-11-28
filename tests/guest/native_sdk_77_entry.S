.section .text.entry,"ax",@progbits
.global _start
_start:
    call sdk77_main
    ud2
.section .text.wait,"ax",@progbits
.global sdk77_wait
.global sdk77_pause
.global sdk77_compare
.global sdk77_branch
sdk77_wait:
    mov $0x12001000,%esi
sdk77_pause:
    pause
sdk77_compare:
    cmpb $0,0x1008(%rsi)
sdk77_branch:
    je sdk77_pause
    ud2
.section .note.GNU-stack,"",@progbits
