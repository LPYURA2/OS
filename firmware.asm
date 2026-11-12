; MATRYOSHKA M32 experimental ROM boot firmware, v0.3.1.
; CPU executes this firmware; host only installs ROM and exposes disk sectors.
; Boot image sector 0 is the header, sectors 1..16 store kernel.bin.
; Header: 'MTRX', length_u32, sector_count_u32, additive_checksum_u32.
; Hardware device OUT 50=sector, OUT 51=destination RAM, OUT 52=read command.
; IN 53 returns 1 after a successful block read; all values 32-bit little endian.
reset_vector:
    MOVI R0, firmware_banner
    MOVI R1, firmware_banner_end-firmware_banner
    WRITE R0, R1
    MOVI R0, 0
    OUT 50, R0
    MOVI R1, 28672
    OUT 51, R1
    MOVI R2, 1
    OUT 52, R2
    IN R3, 53
    CMP R3, R2
    JNZ boot_fail
    ; Validate header magic 'MTRX' = 0x5852544d in LE.
    MOVI R6, 28672
    LOAD R0, R6, 0
    MOVI R1, 1481790541
    CMP R0, R1
    JNZ boot_fail
    ; Number of sectors must fit 0x2000..0x4000, 1..16.
    LOAD R8, R6, 8
    MOVI R9, 1
    CMP R8, R9
    JLT boot_fail
    MOVI R9, 16
    CMP R8, R9
    JGT boot_fail
    ; Kernel byte count positive and <= sectors * 512.
    LOAD R10, R6, 4
    MOVI R9, 1
    CMP R10, R9
    JLT boot_fail
    MOV R9, R8
    MOVI R11, 512
    MUL R9, R11
    CMP R10, R9
    JGT boot_fail
    ; Load kernel bytes *using guest CPU-controlled disk hardware*.
    MOVI R4, 1
    MOVI R5, 8192
    MOVI R7, 0
boot_sector:
    OUT 50, R4
    OUT 51, R5
    OUT 52, R2
    IN R3, 53
    CMP R3, R2
    JNZ boot_fail
    ; Additive checksum of 128 little-endian 32-bit words/sector.
    MOVI R12, 0
    MOVI R13, 128
checksum_word:
    LOAD R14, R5, 0
    ADD R7, R14
    ADDI R5, 4
    ADDI R12, 1
    CMP R12, R13
    JNZ checksum_word
    ADDI R4, 1
    ADDI R8, 4294967295
    MOVI R9, 0
    CMP R8, R9
    JNZ boot_sector
    LOAD R0, R6, 12
    CMP R7, R0
    JNZ boot_fail
    MOVI R0, boot_ok
    MOVI R1, boot_ok_end-boot_ok
    WRITE R0, R1
    JMP 8192
boot_fail:
    MOVI R0, bad_boot
    MOVI R1, bad_boot_end-bad_boot
    WRITE R0, R1
    HALT
firmware_banner:
    .ascii "[FIRMWARE] POST OK - loading kernel from SYSTEM.IMG.\n"
firmware_banner_end:
boot_ok:
    .ascii "[FIRMWARE] Kernel checksum OK - jumping to 0x2000.\n"
boot_ok_end:
bad_boot:
    .ascii "[FIRMWARE] BOOT ERROR: invalid header or checksum.\n"
bad_boot_end:
