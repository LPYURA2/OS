; Self-modifying application. We patch the low byte of immediate operand
; in a real MOVI instruction (8-byte opcode) and execute the patched opcode.
; NOTE: executable writable pages are intentionally enabled for this demo.
start:
    MOVI R4, 65
    MOVI R5, digits
    MOVI R6, 0
again:
    MOVI R1, target+4
    ST8 R4, R1, 0
; Fetch from this address must see the new byte. Original literal is always 65.
target:
    MOVI R0, 65
    MOV R1, R5
    ADD R1, R6
    ST8 R0, R1, 0
    ADDI R4, 1
    ADDI R6, 1
    MOVI R3, 8
    CMP R6, R3
    JLT again
    MOVI R0, line
    MOVI R1, line_end-line
    TRAP 5
    TRAP 3
line:
    .ascii "[selfmod] executed patched instructions: "
digits:
    .ascii "........"
    .ascii "\n"
line_end:
