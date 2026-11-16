> Finalization note, 2026-11-16: this source/model audit was written against the initial draft before the storage protocol gained the separately confirmed inactive-commit invalidation. Its old phase counts and runner-macro blocker describe that draft. The driver itself was unchanged; the macro was corrected and the final counts are init 2053/6/4, update 19/5/3, prepare 14/4/2. See [final independent audit](final-proof-audit.md) and the [final report](../README.md) for the verified implementation and both complete runs. The original observations below are retained.

# T-P1.8A driver audit

Read-only audit of the frozen VirtIO driver/header/model tests, task brief, AGENTS, current ADR-0011, and integration admission/counter paths. No repository files, Git state or refs were changed; no QEMU was run by this auditor. Exact inspected input hashes and host outputs are in `driver-audit.json`.

## Result

No driver correctness blocker found within the explicitly bounded AArch64 QEMU virt contract. One runner integration blocker was independently reproduced: the original transport compile passed `-DSTYLEOS_VBLK_MODEL=1`, while the code requires `STYLEOS_VBLK_HOST_TEST`. It fails under `-Werror`. The parent was notified immediately and assigned its fix to the runner owner. The source runner is being edited concurrently; this report does not certify the final runner.

## Transport evidence

- `initialize` requires MMIO magic/device id=block/version=2; requires offered VERSION_1 and FLUSH, refuses offered RO, selects only VERSION_1 and FLUSH, reads FEATURES_OK back, and validates reset, ready and DRIVER_OK. Unsupported extra offered features are not negotiated. Capacity must be nonzero and at most UINT64_MAX/512; generation-stable reads have 32 tries.
- Queue size is exactly 8 and power-of-two. Descriptor table occupies [0,128), avail flags/index/ring [256,276), used flags/index/ring [512,580) of one 4096-byte aligned page; layouts and alignments do not overlap. Header [0,16), status at64 and bounce [512,1024) are disjoint in the separate request page. One chain/head0 is outstanding at a time. READ has device-write data rights, WRITE data is device-read, FLUSH links directly to status. Status is reset to255 on each submission.
- The real completion path enforces exactly one used-index advance, used id0, device-written lengths READ=513 and WRITE/FLUSH=1, successful status0 and unchanged generation. Errors, needs-reset, FAILED, bad completions and timeout make the engine unusable; failed reads do not copy bounce data to the caller. Requests increment at submission; operation counters increment only on complete success. The total cap8192 prevents counter/index overflow.
- ARM DMB SY orders descriptor/data writes before available-index publication; DSB SY orders publication before MMIO notify. After observing used index, DMB SY precedes used/status/data reads. Cache maintenance is unnecessary under this contract because SCTLR M/C/I are all checked clear; endian bits EE/E0E are checked clear too. Driver changes no MMU/cache/IRQ state.

## Whole-object admission

`storage_demo.c:52-92` checks the complete five new objects before `vb_open` or any new persistent-object initialization: storage control/workspace and driver queue/request/control. It first reuses the read-only boot-pool layout check to validate the diagnostic evidence, without admitting/allocating the pool. It requires complete 4096-byte aligned extents, fixed Image identity/header/full size, RESERVED containment, no CANDIDATE intersection, pairwise disjointness, and separation from stack, handoff, DTB, diagnostic workspace, old pool/metadata and vectors. Only existing image stack locals are written during these checks.

`virtio_block.c:164-203` independently checks all three complete driver objects, full reserved Image identity/header, EL1/SPSel/SP/DAIF/VBAR and MMU/data-cache/instruction-cache/endian state before `initialize` can clear control/queue/request or write device registers. `vb_open` also validates all candidate spans and uniqueness and performs read-only block-device selection before its first writes. Only the admitted arrays reach queue/DMA address programming. Caller data is copied through the fixed bounce area after full-sector Image bounds and overlap checks. This does not provide arbitrary pointer-to-PA translation.

The inherited startup still clears BSS before runtime admission; independent binary proof must bind its complete load/clear extents to the image. The current `storage_site.py` checks full unique zero-filled writable identity LOAD coverage for BSS and full objects, but runtime/DMA evidence remains a separate real-VM obligation.

## Host/model runs

All four executed binaries PASS: GCC14 -O2 existing model; Clang19 -O2 existing model; GCC ASan+UBSan existing model; scratch-only extended model. The extended model runs all8192 legal requests, verifies counters/notifications8192, verifies queue/request/control unchanged on cap refusal, masks extra offered features, accepts the largest safe capacity and refuses perpetually unstable configuration generation. Existing tests cover negotiation/reset/RO/queue/capacity refusals; sector roundtrip/last LBA; modulo8 queue wrap; null/overflow/overlap arguments; unchanged refusal output; id/length/index/status/timeout/reset/config failures and terminal behavior. Host adapter bypasses the ARM physical-placement proof, so none of these runs establish hardware DMA/cache behavior. Sixteen-bit ring-index wrap cannot occur within the cap; the existing wrap means modulo8 slots, and the extended run covers1024 complete slot rotations.

## Expected real request counters

These are derived from the guest storage operations, not observed QEMU evidence. Each successful READ consumes a 513-byte device-written used element; WRITE and FLUSH consume1. The guest/runner should agree on:

| Phase | Reads | Writes | Flushes | Requests |
|---|---:|---:|---:|---:|
| init-a | 2053 | 5 | 3 | 2061 |
| read-a / read-b / recover-a | 14 | 0 | 0 | 14 |
| update-b | 19 | 4 | 2 | 25 |
| prepare-b | 14 | 3 | 1 | 18 |

Init reads every2048 sectors before its first disk write. PREPARED adds3 writes+1 successful flush; COMMITTED adds1 write+1 successful flush. `st_read` adds5 reads; mount on the scenario media adds9. `vb_requests == reads+writes+flushes` is required at successful guest termination.
