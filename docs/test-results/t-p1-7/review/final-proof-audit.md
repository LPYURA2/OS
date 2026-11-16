# T-P1.7 independent final proof, parser and manifest audit

**PASS — no open proof/parser/manifest blocker.** Implementation commit: `af898029ff71892d8e3c6a3308263624af722130`.

The auditor made no Git mutations, remote writes or QEMU runs. The only authorized repository edit was the new `tests/test_user_processes_manifest.py`; all other work was read-only inspection/recomputation and scratch evidence. All 96 source/header/linker/build/test file hashes in `proof-audit-final.json` match the implementation commit. The 24 distinct sources listed by the actual two-architecture compilation sidecars are present in the real build logs and bound to the source snapshot.

## Final executable and UART checks

The auditor independently called the real linked-binary verifier on the final full x86/ARM and ARM-only ARM builds, using actual LLVM 19 decoding. It independently recovered byte-exact raw UART from all five final logs, verified the stored UART SHA256, reparsed each trace, and required exact equality with the corresponding JSON trace and independently rederived site. Manifest, binary, raw Image/disk and site hashes match their exact build folders. No cross-folder substitution is used.

| Final job | A fault PC | Fault syndrome | B saved PC before fault | B resume PC after fault |
|---|---|---|---|---|
| `full/qemu-x86_64-user-processes` | `0x1000002c` | `0x4` | `0x10000039` | `0x10000039` |
| `full/qemu-aarch64-user-processes` | `0x10000040` | `0x92000007` | `0x1000003c` | `0x1000003c` |
| `full/qemu-aarch64-user-processes-el2` | `0x10000040` | `0x92000007` | `0x1000003c` | `0x1000003c` |
| `arm_only_linux/qemu-aarch64-user-processes` | `0x10000040` | `0x92000007` | `0x1000003c` | `0x1000003c` |
| `arm_only_linux/qemu-aarch64-user-processes-el2` | `0x10000040` | `0x92000007` | `0x1000003c` | `0x1000003c` |

Every run has exactly A:YIELD, B:YIELD, A:FAULT, B:EXIT; four real hardware entries; distinct process roots; unchanged SAFE machine state; full 176-byte x86 or 320-byte ARM entry frames inside the independent privileged stack with 16-byte alignment; distinct user stack ranges; exact known load operand VA; and no fifth selection of A. B resumes its saved pre-fault PC/SP, retains its seeded volatile/nonvolatile state and stack-local cookie, observes private `0x123456789abcdef0`, writes `0xfedcba9876543210` and exits. Both slots are released only under SAFE, then final checks, one PASS and ordinary halt occur.

On x86 the observed #PF has CS=0x23, SS=0x1b, vector14, error4 (not present, user read), CR2=0x12010000 and the exact `48 8b 07` MOV load PC. On ARM the lower-EL frame has EL0 SPSR, vector slot8 and ESR=0x92000007 (lower-EL read, valid FAR, L3 translation fault), FAR=0x12010000 and exact `01 00 40 f9` LDR PC. B's private VA is absent under A and present under B.

All five emulator lifecycles have requested_stop=true, settled=true, forced_stop=false, natural_exit=false and returncode=0. Timeout, reset/natural exit, a single PASS or missing terminal evidence is not accepted. The auditor did not substitute host fixtures for these real final logs.

## Independent structural review

The PE proof checks observed runtime relocation against RVAs, whole file-backed user code pages, complete writable/zero-fill resource extents, symbols and nonoverlap. The ARM proof checks complete ELF/NOBITS/PT_LOAD/BSS/raw Image coverage through the established structural verifier. Actual complete executable bytes, including every instruction of both user pages, pass the linked integer-only decoder. Named yield continuations immediately follow actual INT80/SVC1; B exit is actual INT80/SVC2; exact fault bytes and captured operand registers bind the load to the forbidden VA.

The live table parser traverses every used table page under each root and independently enforces exact leaf-set equality. SAFE has no user leaves; A and B each have exactly three: own RX code, own RW/NX stack and own RW/NX private data. Unknown/cyclic/unused parents, huge mappings, extra aliases, foreign private PA aliases and user kernel access are refused. ARM AP/UXN/PXN, device type and both execute dimensions are checked explicitly. Kernel identity aliases remain supervisor-only. Whole-object admission and machine entry correctness are also covered by separate architecture audits.

## Adversarial tests and resolved findings

- 13 binary structural fixture tests pass. These mock only the decoder to exercise malformed PE/ELF data; final linked verification above uses real LLVM.
- 9 trace tests pass, including wrong user origin, PC/VA/root/syndrome/slot, B completion before A fault, resumed A, missing/duplicate records, bad registers/data, table permissions/foreign aliases and truncated or unaligned privileged frames.
- 6 new manifest test methods pass with both-ISA adversarial subcases. They use actual independent PE/ELF/raw byte fixtures and real production structural parsers, mocking only LLVM decode while retaining actual integer compiler-flag validation. They require legitimate manifests first, then reject mode type/value changes, malformed architecture maps, bad target/defines, missing/duplicate/foreign/non-string sources, compiler/site sidecar tampering, every artifact's count/path/bool-size/SHA and changed file bytes.

Earlier review found the parser checked only eight bytes of kernel_sp; root fixed full frame extent plus alignment and added a negative test. Two earlier negative tests accidentally left fixtures unchanged; root corrected their swap and selected the actual user descriptor. Manifest tests exposed unsafe architecture-map/source type assumptions; root added explicit dict/string validation. All are resolved without weakening acceptance checks.

## Final counts and limits

Actual files independently establish baseline52/52 PASS (188 Python tests), final full57/57 PASS and ARM-only43/43 PASS (219 Python tests, OK in each final run). All jobs and hashes are in `proof-audit-final.json`; focused test logs are scratch evidence. The baseline40-artifact preservation and protected historical/remote scope are root/separate-review checks.

This demonstrates only two embedded integer-only user programs on one CPU under the supported QEMU configurations. It does not establish arbitrary-program security, general FP/SIMD context, kernel-fault recovery, IRQ/preemption/SMP, a production process ABI, a general allocator or T-P1.8. Android/Termux, VirtualBox and physical T-P1.7 runs remain separate SKIPPED work until actually run.
