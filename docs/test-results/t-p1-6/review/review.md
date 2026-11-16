# T-P1.6 independent ABI and scope review

Reviewed from base `ea37e76abb0a1b6e75c050c43e357d8c89981537` on `feature/styleos-t-p1-6`. This review makes no repository or Git changes and performs no QEMU runs. Source hashes for the initial adapter/core review are in `review/reviewed-sources.json`; final integration will be reviewed when present.

## Machine context

The x86 adapter uses the actual Microsoft x64 integer call boundary. It preserves RBX, RBP, RDI, RSI and R12–R15. A CALL pushes the continuation at entry RSP; the adapter stores that PC and RSP+8, then restores registers and jumps. Thus resumed contexts have the aligned caller SP, with the caller shadow area intact. The four unused context GPR words are cleared deterministically. RFLAGS arithmetic bits, volatile registers and FP/SIMD are outside the experimental ABI. IF and DF must remain clear.

The initial stack convention `top=end-16; sp=top-40` gives bootstrap entry `RSP % 16 == 8`, a synthetic trap return slot and 32 bytes of shadow space wholly below the upper software guard. The trap consumes that return slot, subtracts 32 and calls the terminal C handler with a valid shadow frame. Probe pushes eight original nonvolatile registers and subtracts 72: its nested CALL is 16-byte aligned, with 32-byte shadow area and saved arguments outside it. Finish and dispatch subtract 40 before calling the switch; their saved continuation SP is aligned, and dispatch restores the frame before RET. No red-zone storage is used.

AArch64 saves/restores X19–X30 plus SP and continuation, including independent restoration of LR before BR. Its probe frame is 128 bytes, dispatch/finish frames 16 bytes, and every SP remains 16-byte aligned. Probe seeds X19–X29 while preserving the original caller register values on its own task stack; LR is tested through the actual labeled continuation and subsequent return. The full expected register masks are FF and 7FF respectively.

The probes check the actual restored machine registers before restoring the caller originals; dispatcher reads of context pattern words are a separate saved-state observation. These are real switches, rather than sequential callbacks or shared-stack coroutine imitation. DONE continuations use explicit failure labels and must never be selected.

## Independent native x86 execution

`review/msabi-host.c` constructs two independent 16384-byte static stacks and the prescribed initial frames, uses GCC `ms_abi` functions, executes six real dispatches, resumes both task C loops twice, verifies FF probe masks, checks each raw saved register pattern, preserves separate task and dispatcher volatile cookies, completes through the assembly finish path and checks both software guard ends. It also binds the dispatcher saved PC to `styleos_tasks_dispatch_resume` and requires aligned saved dispatcher SP.

Result: **PASS**; `review/msabi-host.log`. The assembly instruction stream is unchanged: `review/context-msabi-host.S` differs only by removal of COFF `.def` metadata and addition of an ELF non-executable-stack note. This is Linux host Microsoft-ABI evidence, not a substitute for QEMU or AArch64 execution.

Additional independent execution of `kernel/tasks.c` plus `tests/test_tasks.c` with `-O2 -Wall -Wextra -Werror -fsanitize=address,undefined` passed: `review/host-tasks-sanitized.log`. The integer decoder unit tests passed 12/12 when independently run.

## Read-only architectural observation

The x86 observer retains ADR-0007/0008’s narrow UEFI physical access assumption. Each full table PA page must be RESERVED and outside CANDIDATE before dereference; only the real active root and children reached from real descriptors are used. A bounded identity closure validates visited-table access. Image identity and writable task storage are checked; real GDT VA-to-PA mappings and complete PA-page reservation are established before descriptor reads. No table, GDT, CR3 or translation state is written. This is not a complete firmware dependency enumeration or generic arbitrary-pointer translator.

ARM proves the fixed Image/header range under EL1 MMU off, known vectors, existing dispatcher stack and validated handoff. It probes no general RAM. Architecture unchanged checks retain current inherited x86 mode/root and ARM EL1/MMU-off/VBAR/DAIF.

## Findings caught during development

1. Standalone x86 tasks-site CLI initially called main before defining its PE helper; fixed by moving the entry block.
2. Initial integer whitelist omitted JMP; corrected, and the 12 decoder tests passed.
3. Initial runner expected the old ARM state-field interpretation instead of the new observer’s explicit EL/SPSel/DAIF/VBAR fields; runner agent reports correction.
4. Initial runner demanded saved x86 SP modulo 16 equal 8; corrected to 0 for resumed call continuations. Initial bootstrap entry remains modulo 16 equal 8.
5. Initial runner compared raw arithmetic RFLAGS before/after; corrected to compare IF/DF while requiring all other mode fields unchanged.

No open machine-ABI defect was found in the reviewed adapters. Final demo/admission/runner and final binary evidence remain pending in this interim report.

## Scope observations

Task instructions, AGENTS, ADR-0007/0008 and T-P1.5 report were read. At the initial snapshot main and all old local/remote refs matched the preserved base-era refs; new work was confined to the T-P1.6 branch and no protected boot_info/map/pool/paging/exception source was changed. The presentation `docs/styleos-readme` and Portable-os must remain unchanged. Final scope audit is pending integration.

The result concerns trusted cooperative kernel tasks on one CPU. Software canaries provide no hardware stack isolation; a damaged privileged task can damage the kernel. General FP/SIMD context, IRQ/preemption/SMP, user processes, recovery and the future T-P1.7 user-process fault-containment result are not established. No general RAM ownership transfer is authorized.
