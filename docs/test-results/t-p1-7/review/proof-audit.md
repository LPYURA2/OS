# T-P1.7 independent proof/parser audit (in progress)

Read-only inspection; no repository edits, Git mutations, remote access or QEMU runs.

## Findings requiring root fixes

1. `tools/test.py`, `_user_processes_report`: `kernel_sp` is checked only as an eight-byte interval inside the privileged entry stack. The actual complete raw frame is 176 bytes on x86 and 320 bytes on ARM and is 16-byte aligned. A valid fixture mutated to `entry_stack.end - 8` is accepted on both ISAs. Require the full frame extent and alignment, and add an adversarial fixture. The guest demo already enforces the complete interval, so this is an independent evidence-validator gap, not a demonstrated guest memory bug.

2. Two current negative output tests fail because they construct unchanged fixtures: the sequential `replace(a,b).replace(b,a)` swap cancels itself, and the descriptor lookup chooses the first supervisor identity mapping of data_a rather than the user leaf. Turning an already clear USER/AP bit off is a no-op. Fix fixture construction without weakening parser assertions.

## Checked paths

- PE proof validates image/RVA and file-backed executable whole code pages, zero-fill writable resource extents, old objects, unique symbol placement and nonoverlap. ARM proof validates complete ELF/NOBITS/PT_LOAD/raw Image via the established pool proof before adding full new object extents.
- Real linked-byte LLVM decoder is reused without accepting compiler flags alone. All executable bytes and every user-page instruction are covered; fault opcodes are exactly x86 MOV [RDI] to RAX / ARM LDR X1,[X0]. Yield and exit saved PC are derived from actual adjacent INT80/SVC bytes.
- Runtime parser relocates PE RVAs by observed IMAGE start and binds code-page SHA and site bytes. It requires exact A-yield/B-yield/A-fault/B-exit and no additional STYLEOS records. Hardware frame checks bind real CPL3 CS/SS or EL0 SPSR, expected vector/syndrome, active process root, exact PC/VA and per-slot SP.
- Full table parser traverses every used table from each of three roots, rejects unknown/cyclic/unused parents and unexpected descriptors/leaf sets, and independently verifies the exact kernel-image closure plus three own user leaves. It rejects foreign PA aliases, user kernel access, user writable code and executable data/stack. ARM AP/UXN/PXN and device attributes are checked explicitly.
- Before/after safe machine state is equal; each hardware trap returns to the same original dispatcher SP under SAFE; releases require SAFE; B data evolves only 0 to before-cookie to after-cookie, and a seeded volatile register is checked. The machine implementation must still be assessed by separate arch audit and real QEMU runs.
- The QEMU harness rejects natural exit/reset, absent terminal marker, timeout and insufficient settle interval; it stops intentionally only after collecting the full trace and binds UART/manifest/site SHA into JSON.

## Focused results

- `test_user_processes_site.py`: 13 tests, OK (structural fixture decoder is mocked; not real LLVM evidence).
- `test_user_processes_output.py`: 8 tests, 2 failures from unchanged negative fixtures described above.
- Truncated-frame reproduction: x86 kernel_sp 0x60087ff8 accepted; ARM kernel_sp 0x40107ff8 accepted.

Logs: `proof-audit-site-tests.log`, `proof-audit-output-tests.log` in this scratch directory.
