# StyleFS 50 — scoped engineering acceptance, 2026-11-27

Accepted code baseline: d6ab7a142a61b9448f4e81eb9bbc9f662e13124d, tree 13d616781ae91f698dc50c8b645963b0b81de15d.
Stage 49 f9dba8d5f999089c2620bc883dcc8528fea7dde1: https://github.com/LPYURA2/OS/actions/runs/46021364968 — 12/12 successful after rerun.
Stage 50: https://github.com/LPYURA2/OS/actions/runs/46022240714 — 14/14 successful after rerun, final job enumeration verified. Initial live-volume job 140584529987 failed at apt installation (Microsoft package source HTTP 403), not a guest assertion. Repeat 140605088361 successful without code change.
Exact source https://github.com/LPYURA2/OS/actions/runs/46022240714/artifacts/15208029420
Diagnostic guest ISO and evidence https://github.com/LPYURA2/OS/actions/runs/46022240714/artifacts/15208781460

| Gate | Evidence / result |
|---|---|
| Host models and regressions | host SUCCESS |
| LBA48 block/guest, legacy wide and paged | lba48-block, lba48-guest, wide-guest, paged-guest SUCCESS |
| Object broker and namespace | object-guest, live-namespace SUCCESS; host tests cover directory moves, empty rmdir, file copy and denial |
| Live read/save/reopen | live-volume, live-save SUCCESS |
| Managed ID allocation | live-ids SUCCESS; create 5, rename, delete, cold reopen, recreate 6, damaged receipt refusal |
| Legacy full data >4GiB | large-file SUCCESS, 4GiB+17 |
| Managed 100GiB+17 import and copy | large-paged SUCCESS, see exact log below |
| Guest regression and exact source | guest-regression, source SUCCESS |

Large-paged job 140584529977 raw log:
- 100GiB+17 import committed
- 100GiB+17 copy committed
- 100GiB+17 source verified after reopen
- STYLEFS 50 LARGE SPARSE PASS: 107374182417 bytes each, allocated 13180928 bytes
- Elapsed 28:48.56; maximum resident set size 1676 KiB; exit status 0.

220GiB sparse regular-file backend; logical payload processed completely, source and copy separately verified after close/reopen. Identical zero sectors skipped by test adapter. Not a dense write benchmark, not hardware or guest 100GiB proof; RAM measurement applies only to host test process. Guest proof uses deliberately bounded fixture, not general disk discovery or unrestricted mounts.

## Decision

Accept this scoped baseline for **next application development**. No production release, automatic main merge or assertion of general filesystem maturity. Keep all existing code, branches, evidence and diagnostic profiles. Remaining integration requirements: application-facing authenticated file service, no raw block access for apps, bounded metadata and one-page extent map limits, serialized trusted supervisor, explicit permission selection, no legacy v1 migration, no cross-volume or recursive copy, no full disk snapshot rollback protection, no guarantee of recovery of corrupt v2 watermark (fail closed), no dense physical 100GB validation or broad hardware/VirtualBox certification. Full application volume and user-data backup/recovery need validation before production. Do not interpret absence of test failure as absence of defects.

Next: independent .tea app boundary, then Explorer per owner's new mockups. No UI changes here.
