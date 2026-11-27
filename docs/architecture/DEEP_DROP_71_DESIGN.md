# Deep Drop 71 — design preparation, not accepted implementation

Status: design only, based on stage 70 candidate 5f8720c96f31bd049c2a1e2bd1428e495400c121. Do not start implementation from this ref until stage 70 is accepted; branch new implementation from the final accepted commit and carry this document forward. No changes to main or existing CI.

## Existing functionality to preserve
kernel/desktop_shell.c implements legacy file-to-Notes transfer: transfer.id and disk generation, pointer drag threshold, target hit test, insert_drop checks mounted disk and generation, reads bounded text, refuses insufficient capacity, marks document dirty without saving. Escape cancels. This is useful UX, but source/target logic and buffer are in the shell and do not prove independent interprocess transfer. Existing behavior and approved layouts remain intact.

## Proposed contract
- Shell is a presentation and routing client. A trusted kernel transfer broker owns lifecycle, endpoint identity, bounded metadata and transfer authority. App owns its content and commit semantics. No direct cross-process memory access, forged PIDs or arbitrary filesystem reads by destination.
- Explicit user drag/selection establishes a short-lived, single-transfer authorization. Source PID/generation/root/session, target PID/generation/window/session, allowed type, object identity/revision and COPY action must be verified at the appropriate boundary. No MOVE/delete in v1. Same active session only. Do not grant additional rights through a drop.
- State machine: IDLE -> OFFERED -> TARGET_SELECTED -> ACCEPTED -> DELIVERING -> COMMITTED; cancellation, rejection, stale identity, fault, timeout or revoked permission -> ABORTED with idempotent cleanup. Generation-bound handle prevents replay, no untrusted pointers retained. Duplicate commit never inserts twice.
- Start with bounded UTF-8 text only, explicit byte limits before copying and independent destination capacity validation. Kernel-owned immutable staged bytes or scoped revision reference; no unbounded allocation. Large files/photos/audio/video later use scoped stream, never whole-file copy to RAM. File metadata and actual content distinguished. No automatic save or destructive operation.
- No synchronous wait between apps or shell. Use existing scheduler, event queue/wake mechanisms after stage 70 acceptance. Backpressure, cancellation and bounded memory. Retain current compositor/input semantics and incremental rendering; shell refresh may rebind presentation to live transfer, but must never replay an already committed drop.
- On endpoint crash revoke grant and scrub staging; peer and desktop continue. Preserve already committed destination data and dirty-state semantics. Future draft recovery is separate from incomplete-transfer recovery; no claim that unsaved drafts survive a process crash until independently verified.

## User scenarios, staged
1. Two real native test processes exchange text with source/target consent and identity checks.
2. Actual desktop drag between windows including minimized target activation, cancel, F10 refresh; source and target remain independent, no legacy fallback masquerading as IPC.
3. Explorer/file -> Editor or Notes; selected text -> Create note; browser -> editor and Notes -> presentation, photo drag between applications as those applications and format support become available. No placeholder apps or browser claims.
4. File revision grants and bounded streaming; preserve existing StyleFS large-file/durability acceptance.

## Acceptance
Host positive/negative state-machine tests (stale PID/gen/window, foreign session, revoked source, replay, oversize, bad encoding, rejected/cancelled target, peer fault, repeated cleanup, no side effects on refusal). Then real QEMU CPL3 source and target, actual syscall/IPC receipts, independent PIDs/roots, exactly-once destination commit, source unchanged, F10 during offer, forced-stop each endpoint separately, unrelated peer survives, zero leftover transfer resources after repeated cycles. Keep ordinary ISO regression, 70 wait/wake and forced-stop, hardware exception and storage tests. Report scope, actual test counts, memory high-water and bounded copy size. Do not call entire legacy app migration complete: production Explorer/Notes/Editor/Processes/Documents still need individual .tea migration and grants.

Open design choices: exact bounded payload size, ABI operation numbers, transfer slots, timeout clock and persistence policy require implementation audit against existing kernel APIs. No proprietary source. No code or build change in this preparation.
