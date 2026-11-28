# Native applications 76 — candidate, host coverage first

Base: b6502099077a8ec0b85b4753f46386ae1395f0f8, stage 75 branch preserved, CI 46127568407 (14/14 success).
New candidate extends the existing context and root host tests. It exercises
expanded forced stop after a synthetic timer receipt, stale/unauthorized
refusal, window teardown, peer identity and continuation, and extent removal.
The alias tests cover all 63 splits of a 64-byte window wire across each of
15 interior page boundaries, plus exact end and invalid end-crossing spans.
CI is enabled on the new branch; no production runtime change or new QEMU
forced-stop/IPC proof is claimed. Results pending.

Next: add a separate diagnostic guest scenario for actual expanded forced
stop while a peer remains runnable, and actual NWC_CALL_INPUT across pages
with valid wire bytes and an out-of-range refusal preserving the queue and
destination. Verify exact PID/generation/root, QEMU raw CPL3 events, physical
lease cleanup, neighbor contents, no disk writes and prior 75/Deep Drop
regressions. SDK and real standalone apps remain gated on this work.
Preserve main, all earlier refs, ordinary appearance and the 4 GiB/20 GB
Beta target; idle RAM below 1 GiB is still unmeasured.

Astra / Work: inspect unpublished work and reconcile with the current remote
head; do not reset, overwrite, force-push or redo accepted work.
See docs/NEXT_CHAT_NATIVE_APPS_75.md for earlier checkpoints.

## Implementation 2 — actual guest candidate
Initial stage 76 CI 46128107386: 13 completed jobs successful; StyleFS live
regression still running at observation. Added separate pinned diagnostic
catalog mode ipc76/wide76/peer76 and raw CPL3 verifier. Target performs invalid
64-byte input then valid cross-page input; verifies full returned wire,
end sentinel and peer contents; stops exact target and releases surviving
peers. Independent raw CPU trace, IRQs, zero final leases and zero disk writes
required. Ordinary runtime unchanged. New CI pending; no guest PASS claimed.
Astra: preserve stage 75 and 76 history; inspect any unpublished changes,
reconcile on a separate branch, do not reset or repeat accepted work.
