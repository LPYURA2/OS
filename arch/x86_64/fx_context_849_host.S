/* SysV HOST evidence only: no privileged state changes. Save and restore
 * caller FP/SIMD, perform two real FXSAVE64/FXRSTOR64 round trips. */
.text
.globl s849_host_capture
.type s849_host_capture,@function
s849_host_capture:
    fxsave64 (%rdi)
    ret
.globl s849_host_roundtrip
.type s849_host_roundtrip,@function
/* A, B, observed A, observed B, caller backup, observed A again (all aligned, separate). */
s849_host_roundtrip:
    fxsave64 (%r8)
    fxrstor64 (%rdi)
    fxsave64 (%rdx)
    fxrstor64 (%rsi)
    fxsave64 (%rcx)
    fxrstor64 (%rdi)
    fxsave64 (%r9)
    fxrstor64 (%r8)
    ret
.section .note.GNU-stack,"",@progbits
