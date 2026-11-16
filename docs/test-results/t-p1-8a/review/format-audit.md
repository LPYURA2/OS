# T-P1.8A storage-format audit and repair

Status: PASS; owned source frozen for integration. Read-only audit found one format blocker; parent then explicitly authorized edits only to kernel/storage.c, include/styleos/storage.h and tests/test_storage.c. No Git, remote or QEMU operations were performed.

## Resolved format blocker

The original prepare derived its generation from the active valid version and left inactive commit bytes intact. After commit A(gen1), commit B(gen2), durable payload damage to B, and mount fallback to A, preparing identical B reused generation2. Restoring metadata/payload plus data FLUSH made the old matching gen2 commit valid before st_commit ran. The scratch original reproduction recovered gen2 after prepare only, expected gen1.

The repair first writes an all-zero inactive commit sector and confirms its own FLUSH. Only then does prepare write metadata and both payload sectors and confirm the data FLUSH. Commit separately writes its record and confirms final FLUSH. A complete save has three successful protocol flushes; prepare uses four writes/two flushes. This orders invalidation even when repaired data can become durable immediately or a later operation fails. The disk format is unchanged.

The original empty-file test also passed NULL to memcmp for length0, which UBSan rejected. Parent added its zero-length guard before this repair.

## Verification

- GCC14.2.0 strict C11/O2: PASS, 17 groups.
- Clang19.1.7 strict C11/O2: PASS, 17 groups.
- GCC14 ASan+UBSan, halt_on_error=1, leak detection: PASS, 17 groups.
- Clang19 sanitizer link: unavailable because this downloaded toolchain lacks libclang_rt ASan archives; GCC14 supplied sanitizer coverage.
- New regression confirms corrupt gen2 fallback → identical B reprepare → crash recovers gen1 A; explicit later commit recovers gen2 B.
- Six modeled cuts, after every prepare write/flush, retain complete A. The model lets metadata/payload persist immediately while commit invalidation remains pending until confirmed FLUSH.
- Twelve modeled prepare-failure cases cover all four writes and both flushes, with failed flush either persisting or retaining old durable state. Each unmounts, preserves A and refuses stale tokens.
- A scratch mutant omitting only the invalidation FLUSH is rejected by the isolated adversarial power-cut test because it recovers the wrong generation. This confirms the new behavioral test depends on durability order.

## Other reviewed properties

Explicit init checks exact 2048-sector capacity and observes the entire 1MiB disk as zero before writing; mount never initializes or writes. The fixed little-endian LBA0 superblock and two four-sector slots at2/6 validate version, layout, CRC, generation, identity, bounds, name and all record/payload padding before bounded output copies. Invalid newer records preserve a valid old version; absence of a whole valid version and equal valid generations refuse. Generation overflow and invalid/current/stale token state are rejected. Write/flush uncertainty unmounts and clears pending state.

Limits: host-model/static evidence only. Parent owns transport, guest, runner and actual QEMU integration. This is not physical-device power-loss proof.

## Frozen SHA256

- kernel/storage.c: 8b6c920269d684e61357126988c0ca7416e3ddab125b0de248acc02ff2b60355
- include/styleos/storage.h: 4dafc1cb4312406da787c6b4d1640bcb0d38fa25cf26a0c1416aeaa603f97cfb
- tests/test_storage.c: 32771abf85d4af1089cca67db95e60cde1d3e789236140a463da7a79fb28d540
