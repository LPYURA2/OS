/* Terminal-only workloads: no calls, stack writes, yield or SIMD.
 * Each loop changes only its own counter and arithmetic flags. */
.text
.p2align 4
.globl s843_workload_a,s843_workload_a_end
s843_workload_a:
    incq s843_progress_a(%rip)
    jmp s843_workload_a
s843_workload_a_end:
.p2align 4
.globl s843_workload_b,s843_workload_b_end
s843_workload_b:
    incq s843_progress_b(%rip)
    jmp s843_workload_b
s843_workload_b_end:
.data
.p2align 3
.globl s843_progress_a,s843_progress_b
s843_progress_a: .quad 0
s843_progress_b: .quad 0
