# Stage 93 — explicit file grant receipt candidate

Base 1ec0eaca7f05642751eb8dd92e247f3a7226d67a; 92 guest CI 46134999718 pending at creation. No positive guest read claim yet. One undelivered capability per exact selected owner, trusted supervisor approval only, no app paths/rights accepted. Existing bounded grant slots reused with pending_delivery flag, unusable until one-shot RECEIVE 0x134, zero args, token in RDX, END when empty. No I/O at receipt, lock and process identity checked, dead or revoked grants cleaned by existing reconciliation. Trusted live picker adapter s93_native_file_offer, no production picker UI or user consent interaction implemented; caller must establish real approval. SDK and host tests, 92 diagnostic unchanged. CI pending. Verify 92 and 93; then actual writable guest, explicit selection flow, preemptible disk/stream service, generic .tea and infrastructure acceptance. Preserve main, Astra and all prior refs, boot readiness, optimization, approved UI, ask owner for mockups before product apps.

## 93.1 — inherited 92 harness correction
Stage 92 guest 140948197658 actual CPL3 read and byte checks reached, forced retirement and zero resident/pages/owners/windows/ports observed. Python harness then failed because expected bytes shadowed lifecycle expected list. Rename only diagnostic variable to expected_file; all assertions retained. No positive write guest or final disk unchanged assertion claim yet. Repeat full regression and 93 host approval tests. Previous candidate 10986a3b0699c734025198680727706a18aa0905 remains parent. No main change.

## 94 preparation
93 host job 140948585763 SUCCESS on prior candidate; rerun 93.1 CI 46135174383 still running at observation. Stage 94 separate writable diagnostic candidate, not accepted until real cold reopen and full CI pass.

## Observed evidence
93.1 CI 46135174383: focused native-file-92-guest 140948740246 and native-file-93-host 140948740280 SUCCESS; 27/29 at observation. Actual read with final lifecycle/zero-write checks and host approval mechanism passed. Full regression not yet confirmed. See 94 handoff.
