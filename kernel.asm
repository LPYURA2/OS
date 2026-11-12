; MATRYOSHKA/OS microkernel — runs as ASSEMBLED GUEST CODE at 0x2000.
; The Python emulator executes these instructions; scheduling and syscall
; dispatch below are implemented IN THIS ASSEMBLY, not in a Python OS class.
; R10: boot image cursor, R11: number of programs, R12: scheduler cursor.
boot:
    MOVI R0, boot_msg
    MOVI R1, boot_msg_end-boot_msg
    WRITE R0, R1
    ; The guest kernel, not Python, reads the application catalog.
    MOVI R0, 17
    OUT 50, R0
    MOVI R0, 28672
    OUT 51, R0
    MOVI R3, 1
    OUT 52, R3
    IN R0, 53
    CMP R0, R3
    JNZ app_boot_fail
    MOVI R4, 28672
    LOAD R0, R4, 0
    MOVI R1, 1347436877
    CMP R0, R1
    JNZ app_boot_fail
    LOAD R0, R4, 4
    MOVI R1, 1
    CMP R0, R1
    JNZ app_boot_fail
    LOAD R13, R4, 8
    MOVI R0, 1
    CMP R13, R0
    JLT app_boot_fail
    MOVI R0, 8
    CMP R13, R0
    JGT app_boot_fail
    ; Kernel-owned slot-to-PID table. Never overwrite an executing program.
    MOVI R4, app_pid_table
    MOVI R0, 4294967295
    MOVI R1, 0
init_slot_table:
    STORE R0, R4, 0
    ADDI R4, 4
    ADDI R1, 1
    MOVI R2, 6
    CMP R1, R2
    JNZ init_slot_table
    MOVI R10, 0
    IN R11, 10
load_next:
    CMP R10, R11
    JZ loaded
    ; Port 11 exposes only boot selection (like a firmware boot flag),
    ; not source code, memory addresses or application bytes.
    INIDX R12, 11, R10
    CALL load_program
    MOVI R1, 0
    CMP R0, R1
    JNZ app_boot_fail
    ADDI R10, 1
    JMP load_next
load_program:
    CMP R12, R13
    JGT app_boot_fail
    JZ app_boot_fail
    ; The slot/PID table is maintained by the guest kernel.
    ; An active program MUST be rejected before any disk DMA.
    MOVI R4, app_pid_table
    MOV R0, R12
    MOVI R1, 4
    MUL R0, R1
    ADD R4, R0
    LOAD R0, R4, 0
    MOVI R1, 4294967295
    CMP R0, R1
    JZ load_slot_free
    STATUS R1, R0
    MOVI R0, 1
    CMP R1, R0
    JZ load_slot_busy
load_slot_free:
    ; Catalog: 16-byte header; each entry 40 bytes.
    MOV R4, R12
    MOVI R0, 40
    MUL R4, R0
    ADDI R4, 28688
    LOAD R5, R4, 16
    ; Applications are linked to immutable fixed 12-KiB slots.
    MOV R0, R12
    MOVI R1, 12288
    MUL R0, R1
    ADDI R0, 32768
    CMP R5, R0
    JNZ app_boot_fail
    LOAD R6, R4, 20
    LOAD R7, R4, 24
    LOAD R8, R4, 28
    LOAD R9, R4, 32
    MOVI R0, 18
    CMP R6, R0
    JLT app_boot_fail
    MOVI R0, 1
    CMP R7, R0
    JLT app_boot_fail
    MOVI R0, 24
    CMP R7, R0
    JGT app_boot_fail
    MOV R0, R6
    ADD R0, R7
    MOVI R1, 128
    CMP R0, R1
    JGT app_boot_fail
    MOVI R0, 1
    CMP R8, R0
    JLT app_boot_fail
    MOVI R0, 12288
    CMP R8, R0
    JGT app_boot_fail
    MOV R0, R7
    MOVI R1, 512
    MUL R0, R1
    CMP R8, R0
    JGT app_boot_fail
    MOV R14, R5
    MOVI R15, 0
app_sector:
    OUT 50, R6
    OUT 51, R14
    MOVI R3, 1
    OUT 52, R3
    IN R0, 53
    CMP R0, R3
    JNZ app_boot_fail
    MOVI R1, 0
    MOVI R2, 128
app_checksum_word:
    LOAD R0, R14, 0
    ADD R15, R0
    ADDI R14, 4
    ADDI R1, 1
    CMP R1, R2
    JNZ app_checksum_word
    ADDI R6, 1
    ADDI R7, 4294967295
    MOVI R0, 0
    CMP R7, R0
    JNZ app_sector
    CMP R15, R9
    JNZ app_boot_fail
    SPAWN R5
    MOVI R0, 4294967295
    CMP R5, R0
    JZ load_slot_busy
    MOVI R4, app_pid_table
    MOV R0, R12
    MOVI R1, 4
    MUL R0, R1
    ADD R4, R0
    STORE R5, R4, 0
    MOVI R0, 0
    RET
load_slot_busy:
    MOVI R0, 4294967295
    RET
app_boot_fail:
    MOVI R0, app_error
    MOVI R1, app_error_end-app_error
    WRITE R0, R1
    HALT
loaded:
    MOVI R12, 7
schedule:
    IN R6, 12
    MOVI R7, 0
    CMP R6, R7
    JZ shutdown
    ADDI R12, 1
    MODI R12, 8
    STATUS R0, R12
    MOVI R1, 1
    CMP R0, R1
    JNZ schedule
    SWITCH R12
    ; This instruction is resumed after a user syscall or timer interrupt.
    JMP handle_trap
handle_trap:
    IN R4, 0
    MOVI R5, 1
    CMP R4, R5
    JZ sys_putchar
    MOVI R5, 3
    CMP R4, R5
    JZ sys_exit
    MOVI R5, 5
    CMP R4, R5
    JZ sys_write
    MOVI R5, 6
    CMP R4, R5
    JZ sys_block
    MOVI R5, 7
    CMP R4, R5
    JZ sys_status
    MOVI R5, 8
    CMP R4, R5
    JZ sys_getkey
    MOVI R5, 9
    CMP R4, R5
    JZ sys_clear
    MOVI R5, 10
    CMP R4, R5
    JZ sys_exec
    ; 0=timer, 2=yield, other=ignored
    JMP schedule
sys_putchar:
    IN R0, 2
    OUT 1, R0
    JMP schedule
sys_exit:
    IN R0, 4
    KILL R0
    JMP schedule
sys_write:
    IN R0, 2
    IN R1, 3
    WRITE R0, R1
    JMP schedule
sys_block:
    IN R0, 2
    IN R1, 3
    IN R2, 5
    BLOCK R0, R1, R2
    JMP schedule
sys_status:
    IN R0, 2
    STATUS R1, R0
    SETRET R1
    JMP schedule
sys_getkey:
    IN R0, 30
    SETRET R0
    JMP schedule
sys_clear:
    MOVI R0, 0
    OUT 40, R0
    JMP schedule
; Guest user-mode syscall 10: run an application ID from SYSTEM.IMG.
; R0=index, result R0=0 on success, -1 for invalid ID or exhausted PID slots.
; Preserve scheduler cursor R12 while loading from disk.
sys_exec:
    MOV R10, R12
    IN R12, 2
    CMP R12, R13
    JGT sys_exec_failed
    JZ sys_exec_failed
    MOVI R0, 5
    CMP R12, R0
    JZ sys_exec_failed
    IN R0, 13
    MOVI R1, 8
    CMP R0, R1
    JZ sys_exec_failed
    CALL load_program
    SETRET R0
    MOV R12, R10
    JMP schedule
sys_exec_failed:
    MOVI R0, 4294967295
    SETRET R0
    MOV R12, R10
    JMP schedule
shutdown:
    MOVI R0, shutdown_msg
    MOVI R1, shutdown_msg_end-shutdown_msg
    WRITE R0, R1
    HALT
boot_msg:
    .ascii "[KERNEL] MATRYOSHKA/OS guest kernel booted.\n"
boot_msg_end:
shutdown_msg:
    .ascii "[KERNEL] All guest tasks exited. CPU HALT.\n"
shutdown_msg_end:

app_error:
    .ascii "[KERNEL] APP LOAD ERROR: catalog, sector or checksum.\n"
app_error_end:

; Six 32-bit PIDs of the process currently owning each executable slot.
; 0xFFFFFFFF means unused; dead PIDs may be replaced safely.
app_pid_table:
    .zero 24
