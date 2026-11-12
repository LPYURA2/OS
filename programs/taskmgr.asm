; Native guest task manager: read process STATUS via syscall 7, print atomic lines.
start:
    MOVI R3, 0
loop:
    MOV R0, R3
    TRAP 7
    MOVI R1, 1
    CMP R0, R1
    JNZ advance
    MOVI R4, line
    MOV R0, R3
    ADDI R0, 48
    ST8 R0, R4, digit-line
    MOVI R0, line
    MOVI R1, line_end-line
    TRAP 5
advance:
    ADDI R3, 1
    MOVI R1, 8
    CMP R3, R1
    JLT loop
    TRAP 3
line:
    .ascii "[ps] PID "
digit:
    .ascii "0 is RUNNING\n"
line_end:
