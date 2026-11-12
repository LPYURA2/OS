; User-mode hello, built by our assembler and executed by our CPU.
start:
    MOVI R0, message
    MOVI R1, message_end-message
    TRAP 5
    TRAP 3
message:
    .ascii "[hello] Hello from USER MODE on a Python-built CPU!\n"
message_end:
