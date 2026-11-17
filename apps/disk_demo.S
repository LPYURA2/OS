/* Build this file separately with Clang/LLD; it is never a kernel input.
 * The two immediates are fixture arguments, not a production application ABI. */
#ifndef APP_MARKER
#define APP_MARKER 0x1818a001
#endif
#ifndef APP_EXIT
#define APP_EXIT 41
#endif
.section .text.app,"ax",%progbits
.balign 4
.global disk_app_entry, disk_app_fault, disk_app_normal, disk_app_exit, disk_app_end
disk_app_entry:
    cbnz x0, disk_app_normal
    movz x1, #0x1201, lsl #16
disk_app_fault:
    ldr x2, [x1]
disk_app_normal:
    movz x19, #(APP_MARKER & 0xffff)
    movk x19, #((APP_MARKER >> 16) & 0xffff), lsl #16
    movz x0, #APP_EXIT
disk_app_exit:
    svc #2
disk_app_end:
