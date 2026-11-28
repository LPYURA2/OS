# Stage 75 — multi-page CPL3 preemption

The tested source is `691d4299e98dd50f0d47919c4d6323040dd9067a`.
All 14 jobs passed, with zero failures and zero skipped jobs.
[Source](https://github.com/LPYURA2/OS/tree/691d4299e98dd50f0d47919c4d6323040dd9067a) ·
[GitHub Actions](https://github.com/LPYURA2/OS/actions/runs/46127161578)

| Observation | Result |
| --- | --- |
| QEMU 8.2.2, x86-64, one vCPU, 128 MiB | 18 ordered CPL3 events matched UART and CPU trace |
| Interrupts in the wait loops before release | 4 in `wide75`, 3 in `fault75`, 2 in `peer75`, each with a peer running between slices |
| Terminal events | `wide75` exited, `fault75` reached the intended #UD, the legacy peer yielded after the fault and exited |
| Guest process memory | 28,672 + 28,672 + 12,288 bytes in owned leases, zero pages left in the arena |
| Data disk | QMP recorded zero write operations and bytes, backing hash matched |
| Host GCC ASan/UBSan | 81,084 ELF/lease checks, 1,035 root checks, 47 context checks passed |
| Host Clang ASan/UBSan | 81,084 ELF/lease checks, 843 root checks, 47 context checks passed |

Two local guest boots reproduced the same preemption counts. The original
failure also reproduced locally on the parent commit. On a local replay the
verifier rejected a removed raw IRQ, a software-marked IRQ, a changed CR3,
an early release and a missing LAPIC retirement record. The CI guest trace
and UART were downloaded and replayed independently after the run.
[Guest logs and programs](https://github.com/LPYURA2/OS/actions/runs/46127161578/artifacts/15256470655) ·
[Host results](https://github.com/LPYURA2/OS/actions/runs/46127161578/artifacts/15256695158)

| Verified build | SHA-256 of the unpacked ISO |
| --- | --- |
| [Ordinary desktop ISO](https://github.com/LPYURA2/OS/actions/runs/46127161578/artifacts/15255808978) | `526345f4c6dcd627a59ee9ff3ed43b3dee167532ab226c525cbf9e6fd47b79ca` |
| [Expanded diagnostic ISO and data disk](https://github.com/LPYURA2/OS/actions/runs/46127161578/artifacts/15256355770) | `775346f05f840045d1f0f9c9000a967c61fb5c79733dfa3f124921abfb8ed3f9` |

The ordinary ISO also passed the existing Deep Drop guest checks.
The expanded workloads run on the diagnostic ISO with the accompanying disk.
The measured allocations cover the three user processes. Whole-system idle
RAM and physical hardware are outside this run.
