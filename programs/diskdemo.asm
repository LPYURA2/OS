; Host-backed block device; kernel receives TRAP 6 and issues BLOCK hardware op.
; First write sector 3, then read it into ANOTHER RAM address and print proof.
start:
    MOVI R0, 3
    MOVI R1, buffer
    MOVI R2, 1
    TRAP 6
    MOVI R0, 3
    MOVI R1, read_buffer
    MOVI R2, 0
    TRAP 6
    MOVI R0, read_buffer
    MOVI R1, msg_end-buffer
    TRAP 5
    TRAP 3
buffer:
    .ascii "DISK: persistent data written by guest OS.\n"
msg_end:
    .zero 512-(msg_end-buffer)
read_buffer:
    .zero 512
