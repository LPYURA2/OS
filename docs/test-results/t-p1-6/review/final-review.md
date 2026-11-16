# T-P1.6 independent final review

**PASS — no open acceptance blocker found.** The reviewed implementation is
`d8065f31ae292eccd2558ddc8dc2cd91b78b855f` on
`feature/styleos-t-p1-6`, based on
`ea37e76abb0a1b6e75c050c43e357d8c89981537`.

This review made no repository edits, Git mutations, remote writes or QEMU
runs. It inspected the task brief, AGENTS, ADR-0009, prior independent review,
current sources, final builds and their actual evidence. Independent delegated
reviews covered the machine ABI/admission/lifecycle and linked-byte/UART
proof paths. Only this scratch report, JSON audit and read-only remote-ref
snapshots were written. Root owns final pinned documentation, report and
publication; their subsequent addition does not invalidate unchanged code.

## Machine context and lifecycle

The x86 switch preserves RBX, RBP, RDI, RSI and R12–R15, records the CALL
continuation from `[RSP]` and its caller SP as `RSP+8`, restores the target
context and jumps to its PC. The resumed SP is 16-byte aligned. The initial
usable top excludes the 16-byte upper software canary; `top-40` supplies an
8-modulo-16 function-entry SP, synthetic trap return and full 32-byte shadow
space. Probe, dispatch, finish and trap frames satisfy Microsoft x64 alignment
and shadow-space requirements. The profile disables the red zone. Arithmetic
flags and volatile registers are outside this call-boundary contract; IF/DF
remain clear. The four unused GPR words are deterministic zeroes.

AArch64 saves/restores X19–X30, SP and the actual BL continuation, restores LR
independently and branches to the saved PC. Initial, probe, dispatch and finish
frames keep SP aligned to 16. X19–X29 are seeded and checked live after real
resume; X30 is checked against the linked continuation. The expected live masks
are x86 `0xff` and ARM `0x7ff`. Saved-context pattern observation and live
register comparison are separate checks.

The previously completed native Linux Microsoft-ABI execution independently
performed six real switches on two separate static stacks, checked live masks,
saved raw patterns, dispatcher/task volatile cookies, continuations, finish and
canaries. Its log and source are SHA-bound in `final-review.json`. It remains
host ABI evidence, separate from QEMU execution. No test rerun was necessary.

Admission establishes complete image-owned, RESERVED, correctly aligned and
nonoverlapping stack/control ranges, their exclusion from all declared live
objects and per-architecture address/writability evidence before new task
storage writes. The boot pool is present but not admitted or allocated.
The x86 observer reads only fully admitted table/GDT pages under the documented
bounded inherited UEFI mapping contract; ARM remains EL1 with MMU off.

The C bookkeeping engine rejects malformed control, null/aligned/aliasing
outputs, bad stacks, overlaps, invalid transitions and counter overflow before
mutation. Round robin chooses READY only. Each entry returns to bootstrap,
which marks DONE and transfers through finish. DONE cannot be selected; its
saved continuation and the initial return trap are explicit failure paths.
EMPTY returns to the original dispatcher stack. Release clears bookkeeping
without freeing either static stack.

## Linked proof and genuine final runs

The independent proof reviewer recomputed `verified_tasks_site` on the final
linked PE/ELF/Image with real LLVM 19.1.7, then reconstructed and reparsed the
raw UART byte-exactly from each final log. All three full-run JSON payloads
match the current site, manifest SHA, UART SHA and complete parsed trace:

| Final full job | Result | Linked integer decode |
|---|---|---|
| `qemu-x86_64-tasks` | PASS | 32,166 bytes; 8,340 instructions |
| `qemu-aarch64-tasks` | PASS | 32,012 bytes; 7,535 instructions |
| `qemu-aarch64-tasks-el2` | PASS | Same ARM linked image; independent EL2-to-EL1 UART |

The decoder proves gap-free raw-byte agreement across every actual executable
section, refuses bad decode, overlap, gaps, unsupported FP/SIMD instructions
and conflicting compiler flags, and binds named entry/continuation sites to
decoded instruction boundaries. The ARM entry branch is included; only the
validated 60 bytes of Linux Image header data are excluded. Declared all-zero
ARM vector padding accounts for 1,872 bytes. Direct CALL/BL instructions just
before the three saved continuations target the same real switch.

Every task run has exactly
`A:START, B:START, A:RESUME, B:RESUME, A:EXIT, B:EXIT`, six GO operations,
two tagged yields per task, matching raw saved registers and live masks,
distinct stable stack-local cookie addresses/values, correct saved SP/PC,
two normal entry returns/DONE states, unchanged original dispatcher stack and
cookie, EMPTY with completed=2/dispatches=6, two releases, intact canaries,
matching before/after architectural observations, one TASKS PASS and ordinary
halt. The strict parser rejects missing/duplicate/reordered task records,
out-of-range SP, wrong cookie/pattern/mask/PC, repeats after EXIT and PASS-only.

All three final full jobs and both final ARM-only task jobs have
`requested_stop=true`, `forced_stop=false`, `natural_exit=false`,
`settled=true`, `returncode=0`. The harness observed the live emulator for
200 ms after the terminal marker before intentional stop. Timeout, reset,
silence or natural process exit did not substitute for this result.

## Regression and scope audit

Actual final files establish:

| Evidence set | Actual result |
|---|---|
| Base rerun | 47/47 PASS |
| Full Linux `build/t-p1-6` | 52/52 PASS |
| ARM-only Linux `build/termux-tp16` | 39/39 PASS |
| Final Python suites | 188 tests, OK in each final run |
| Ordinary reproducibility | Four artifacts identical in independent builds |
| Required previous seven-profile set | 28/28 artifacts identical to same-toolchain baseline |
| All nine previous profiles including both T-P1.5 ownership profiles | 36/36 artifacts identical; TASKS marker absent in all |

The JSON audit records every artifact's baseline/current SHA and all final
task UART/log/JSON/site/manifest hashes. It also records all 22 implementation
files' SHA256 values; the source snapshot digest is
`0e562bab1451afc9abe7170c5a62694a784ce06f9c724375fdec04661ea7123b`.

Of 576 base files, 568 are byte-identical and eight existing files have only
the authorized integration/documentation/test-inventory changes. Fourteen
files were added for task implementation, tests, proof and ADR-0009. There
are no missing base files. All 490 historical evidence files and the protected
presentation README are unchanged. The sole ownership-test edit is the
mechanical build inventory `9` to `10`. Boot-info, memory normalization, pool,
paging, ownership implementation, exception and startup source contracts are
unchanged; only conditional `kernel/main.c` integration activates the new
profile. Exact old artifact equality additionally establishes profile isolation.

Read-only final `ls-remote --symref` snapshots for both OS and Portable-os
are byte-identical to their before-task snapshots, including main,
`docs/styleos-readme` and all previous remote refs. The local implementation
commit is on the new task branch. Portable-os has no worktree changes.

## Limits and remaining root-owned work

These are trusted cooperative privileged kernel functions on one CPU with an
integer-only call-boundary context. Software canaries do not provide hardware
stack protection, task isolation or recovery. No general FP/SIMD context,
PMM/heap, CANDIDATE/REVIEW access, RAM grant/reclaim, IRQ/preemption/SMP,
new CR3/MMU/GDT policy, user mode, syscalls or IPC is established. The future
T-P1.7 user-process fault-containment outcome is not claimed.

The owner's T-P1.5 Termux 35/35, exit-0 QEMU 11.1.3 screenshot report is a
separate historical owner report. T-P1.6 Android/Termux, VirtualBox and physical
hardware remain SKIPPED. Final report/pinned commands and branch-only
publication remain root-owned finalization; no additional feature work is
needed or authorized by this review.
