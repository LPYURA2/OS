; A competing task, preempted by timer, with atomic per-line console writes.
start:
    MOVI R2, 1
again:
    MOVI R4, line
    MOV R0, R2
    ADDI R0, 48
    ST8 R0, R4, digit-line
    MOVI R0, line
    MOVI R1, line_end-line
    TRAP 5
    TRAP 2
    ADDI R2, 1
    MOVI R3, 6
    CMP R2, R3
    JLT again
    TRAP 3
line:
    .ascii "[counter] counter: "
digit:
    .ascii "0\n"
line_end:
