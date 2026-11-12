; Guest OS console. Every instruction here executes on virtual ISA CPU.
; The Python host supplies keyboard bytes via input port 30; the guest
; kernel mediates syscalls, and the screen is a Python-emulated MMIO device.
start:
    MOVI R0, banner
    MOVI R1, banner_end-banner
    TRAP 5
prompt:
    MOVI R0, prompt_msg
    MOVI R1, prompt_msg_end-prompt_msg
    TRAP 5
    MOVI R3, 0
readkey:
    TRAP 8
    MOVI R1, 0
    CMP R0, R1
    JZ readkey
    MOVI R1, 10
    CMP R0, R1
    JZ dispatch
    MOVI R1, 95
    CMP R3, R1
    JZ readkey
    MOVI R4, buffer
    ADD R4, R3
    ST8 R0, R4, 0
    ADDI R3, 1
    TRAP 1
    JMP readkey
dispatch:
    TRAP 1
    MOVI R1, 0
    CMP R3, R1
    JZ prompt
    ; help
    MOVI R1, 4
    CMP R3, R1
    JNZ check_ps
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 104
    CMP R0, R1
    JNZ check_ps
    LD8 R0, R4, 1
    MOVI R1, 101
    CMP R0, R1
    JNZ check_ps
    LD8 R0, R4, 2
    MOVI R1, 108
    CMP R0, R1
    JNZ check_ps
    LD8 R0, R4, 3
    MOVI R1, 112
    CMP R0, R1
    JZ help_command
check_ps:
    MOVI R1, 2
    CMP R3, R1
    JNZ check_info
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 112
    CMP R0, R1
    JNZ check_info
    LD8 R0, R4, 1
    MOVI R1, 115
    CMP R0, R1
    JZ ps_command
check_info:
    MOVI R1, 4
    CMP R3, R1
    JNZ check_clear
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 105
    CMP R0, R1
    JNZ check_clear
    LD8 R0, R4, 1
    MOVI R1, 110
    CMP R0, R1
    JNZ check_clear
    LD8 R0, R4, 2
    MOVI R1, 102
    CMP R0, R1
    JNZ check_clear
    LD8 R0, R4, 3
    MOVI R1, 111
    CMP R0, R1
    JZ info_command
check_clear:
    MOVI R1, 5
    CMP R3, R1
    JNZ check_exit
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 99
    CMP R0, R1
    JNZ check_exit
    LD8 R0, R4, 1
    MOVI R1, 108
    CMP R0, R1
    JNZ check_exit
    LD8 R0, R4, 2
    MOVI R1, 101
    CMP R0, R1
    JNZ check_exit
    LD8 R0, R4, 3
    MOVI R1, 97
    CMP R0, R1
    JNZ check_exit
    LD8 R0, R4, 4
    MOVI R1, 114
    CMP R0, R1
    JZ clear_command
check_exit:
    MOVI R1, 4
    CMP R3, R1
    JNZ check_save
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 101
    CMP R0, R1
    JNZ check_save
    LD8 R0, R4, 1
    MOVI R1, 120
    CMP R0, R1
    JNZ check_save
    LD8 R0, R4, 2
    MOVI R1, 105
    CMP R0, R1
    JNZ check_save
    LD8 R0, R4, 3
    MOVI R1, 116
    CMP R0, R1
    JZ exit_command

; NOTE.TXT experiment: one 90-byte record on sector 80.
; Only virtual CPU instructions process the command and build the block.
check_save:
    MOVI R1, 5
    CMP R3, R1
    JLT check_load
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 115
    CMP R0, R1
    JNZ check_load
    LD8 R0, R4, 1
    MOVI R1, 97
    CMP R0, R1
    JNZ check_load
    LD8 R0, R4, 2
    MOVI R1, 118
    CMP R0, R1
    JNZ check_load
    LD8 R0, R4, 3
    MOVI R1, 101
    CMP R0, R1
    JNZ check_load
    LD8 R0, R4, 4
    MOVI R1, 32
    CMP R0, R1
    JZ save_command
check_load:
    MOVI R1, 4
    CMP R3, R1
    JNZ check_ls
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 108
    CMP R0, R1
    JNZ check_ls
    LD8 R0, R4, 1
    MOVI R1, 111
    CMP R0, R1
    JNZ check_ls
    LD8 R0, R4, 2
    MOVI R1, 97
    CMP R0, R1
    JNZ check_ls
    LD8 R0, R4, 3
    MOVI R1, 100
    CMP R0, R1
    JZ load_command
check_ls:
    MOVI R1, 2
    CMP R3, R1
    JNZ check_rm
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 108
    CMP R0, R1
    JNZ check_rm
    LD8 R0, R4, 1
    MOVI R1, 115
    CMP R0, R1
    JZ ls_command
check_rm:
    MOVI R1, 2
    CMP R3, R1
    JNZ check_run
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 114
    CMP R0, R1
    JNZ check_run
    LD8 R0, R4, 1
    MOVI R1, 109
    CMP R0, R1
    JZ rm_command
check_run:
    MOVI R1, 5
    CMP R3, R1
    JNZ unknown
    MOVI R4, buffer
    LD8 R0, R4, 0
    MOVI R1, 114
    CMP R0, R1
    JNZ unknown
    LD8 R0, R4, 1
    MOVI R1, 117
    CMP R0, R1
    JNZ unknown
    LD8 R0, R4, 2
    MOVI R1, 110
    CMP R0, R1
    JNZ unknown
    LD8 R0, R4, 3
    MOVI R1, 32
    CMP R0, R1
    JNZ unknown
    LD8 R0, R4, 4
    MOVI R1, 48
    CMP R0, R1
    JLT launch_invalid
    MOVI R1, 52
    CMP R0, R1
    JGT launch_invalid
    ADDI R0, 4294967248
    TRAP 10
    MOVI R1, 0
    CMP R0, R1
    JNZ launch_failed
    MOVI R0, started_msg
    MOVI R1, started_end-started_msg
    TRAP 5
    JMP prompt
launch_invalid:
    MOVI R0, invalid_run_msg
    MOVI R1, invalid_run_end-invalid_run_msg
    TRAP 5
    JMP prompt
launch_failed:
    MOVI R0, failed_run_msg
    MOVI R1, failed_run_end-failed_run_msg
    TRAP 5
    JMP prompt
unknown:
    MOVI R0, unknown_msg
    MOVI R1, unknown_end-unknown_msg
    TRAP 5
    JMP prompt
help_command:
    MOVI R0, help_msg
    MOVI R1, help_end-help_msg
    TRAP 5
    JMP prompt
info_command:
    MOVI R0, info_msg
    MOVI R1, info_end-info_msg
    TRAP 5
    JMP prompt
clear_command:
    TRAP 9
    JMP prompt
exit_command:
    MOVI R0, bye_msg
    MOVI R1, bye_end-bye_msg
    TRAP 5
    TRAP 3
; Disk record: [magic MNT1][one length byte][payload][remaining zeros].
; NOTE.TXT is a first block-storage experiment, not a real filesystem.
save_command:
    MOVI R4, sector_data
    MOVI R0, 77
    ST8 R0, R4, 0
    MOVI R0, 78
    ST8 R0, R4, 1
    MOVI R0, 84
    ST8 R0, R4, 2
    MOVI R0, 49
    ST8 R0, R4, 3
    MOV R6, R3
    MOVI R7, 5
    SUB R6, R7
    ST8 R6, R4, 4
    MOVI R5, 0
save_copy_loop:
    CMP R5, R6
    JZ save_commit
    MOVI R8, buffer+5
    ADD R8, R5
    LD8 R0, R8, 0
    MOVI R9, sector_data+5
    ADD R9, R5
    ST8 R0, R9, 0
    ADDI R5, 1
    JMP save_copy_loop
save_commit:
    MOVI R0, 80
    MOVI R1, sector_data
    MOVI R2, 1
    TRAP 6
    MOVI R0, saved_msg
    MOVI R1, saved_end-saved_msg
    TRAP 5
    JMP prompt
load_command:
    CALL read_note
    MOVI R1, 1
    CMP R0, R1
    JNZ no_note
    MOVI R4, sector_data
    LD8 R1, R4, 4
    MOVI R0, sector_data+5
    TRAP 5
    MOVI R0, newline_msg
    MOVI R1, newline_end-newline_msg
    TRAP 5
    JMP prompt
ls_command:
    CALL read_note
    MOVI R1, 1
    CMP R0, R1
    JNZ no_note
    MOVI R0, exists_msg
    MOVI R1, exists_end-exists_msg
    TRAP 5
    JMP prompt
no_note:
    MOVI R0, empty_msg
    MOVI R1, empty_end-empty_msg
    TRAP 5
    JMP prompt
rm_command:
    MOVI R4, sector_data
    MOVI R0, 0
    ST8 R0, R4, 0
    MOVI R0, 80
    MOVI R1, sector_data
    MOVI R2, 1
    TRAP 6
    MOVI R0, removed_msg
    MOVI R1, removed_end-removed_msg
    TRAP 5
    JMP prompt
read_note:
    MOVI R0, 80
    MOVI R1, sector_data
    MOVI R2, 0
    TRAP 6
    MOVI R4, sector_data
    LD8 R0, R4, 0
    MOVI R1, 77
    CMP R0, R1
    JNZ invalid_note
    LD8 R0, R4, 1
    MOVI R1, 78
    CMP R0, R1
    JNZ invalid_note
    LD8 R0, R4, 2
    MOVI R1, 84
    CMP R0, R1
    JNZ invalid_note
    LD8 R0, R4, 3
    MOVI R1, 49
    CMP R0, R1
    JNZ invalid_note
    LD8 R0, R4, 4
    MOVI R1, 91
    CMP R0, R1
    JLT valid_note
    JMP invalid_note
valid_note:
    MOVI R0, 1
    RET
invalid_note:
    MOVI R0, 0
    RET
ps_command:
    MOVI R0, ps_title
    MOVI R1, ps_title_end-ps_title
    TRAP 5
    MOVI R5, 0
ps_loop:
    MOV R0, R5
    TRAP 7
    MOVI R1, 1
    CMP R0, R1
    JNZ ps_next
    MOVI R4, ps_line_digit
    MOV R0, R5
    ADDI R0, 48
    ST8 R0, R4, 0
    MOVI R0, ps_line
    MOVI R1, ps_line_end-ps_line
    TRAP 5
ps_next:
    ADDI R5, 1
    MOVI R1, 8
    CMP R5, R1
    JLT ps_loop
    JMP prompt
banner:
    .ascii "MATRYOSHKA/OS 0.3.2 - DISK LOADED GUEST SHELL\nType help for commands.\n"
banner_end:
prompt_msg:
    .ascii "M:> "
prompt_msg_end:
help_msg:
    .ascii "help ps info clear exit\nls load save <text> rm\nrun 0..4 (hello counter taskmgr diskdemo selfmod)\n"
help_end:
info_msg:
    .ascii "CPU: MATRYOSHKA-32, 16 regs, 256 KiB RAM\nOS: guest assembly kernel, timer preemption\nKEYBOARD: host-fed port 30, DISPLAY: host-emulated\nNOTE: sector 80, max 90 UTF-8 bytes\n"
info_end:
started_msg:
    .ascii "Application launched from system disk.\n"
started_end:
invalid_run_msg:
    .ascii "Unknown app ID. Use run 0..4.\n"
invalid_run_end:
failed_run_msg:
    .ascii "Unable to launch: app busy or task table full.\n"
failed_run_end:
unknown_msg:
    .ascii "Unknown command. Type help.\n"
unknown_end:
bye_msg:
    .ascii "Shell process exiting.\n"
bye_end:
ps_title:
    .ascii "PID   STATE\n"
ps_title_end:
ps_line:
    .ascii "  "
ps_line_digit:
    .ascii "0    RUN\n"
ps_line_end:
.zero 8
buffer:
    .zero 96

saved_msg:
    .ascii "Saved NOTE.TXT on virtual disk.\n"
saved_end:
exists_msg:
    .ascii "NOTE.TXT\n"
exists_end:
empty_msg:
    .ascii "No NOTE.TXT (save <text> first).\n"
empty_end:
removed_msg:
    .ascii "NOTE.TXT removed.\n"
removed_end:
newline_msg:
    .ascii "\n"
newline_end:
; Sector buffer belongs to the shell user-process memory slot.
sector_data:
    .zero 512
