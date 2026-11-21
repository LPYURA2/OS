# StyleOS 767 — kernel-native app staging transaction

Base: 766 (`a95b0b584ccfb5474383c3a4afe48dfa23bf111c`, CI PASS).
Task branch: `feature/styleos-native-launch-transaction-767`.

## Goal

Prepare *atomic, kernel-private* bookkeeping for a native application between
independent, already verified subsystem boundaries. This is NOT a new shell
window masquerading as a process, and not a package loader, syscall or
hardware-backed application launch.

### Transaction

`nla_init` takes three already initialized, separate services:

- `dpr_service`: kernel process registry with scheduler slots and true PID;
- `dfo_owner`: prebuilt detached SAFE + USER-A/USER-B tables and independent
  **external** proof that all physical pages are owned/reserved;
- `usr_runtime`: trusted session state and independent window registry.

`nla_commit` MUST be invoked by a trusted, single-core loader only AFTER it has
verified the native `.tea` package/manifest, loaded ELF64, checked the code
entry and permissions, populated/validated real physical code/stack/data,
constructed the actual architecture context and proved the complete kernel
closure (IDT/GDT/TSS/IRQ/stack/framebuffer/MMIO) in the candidate root.
**None of those prerequisites is performed or asserted as true by this module.**
A caller claiming an observed CR3 or passing a context handle is not evidence
of a true hardware mapping. The entry points must stay unreachable to CPL3.

In order, the transaction:

1. Refuses an active scheduler, unsafe current root, consumed slot or missing
   session. Does not fabricate a user/profile/session.
2. Admits the supplied **prepared kernel context** into the kernel scheduler,
   which allocates a monotonically increasing real PID, initially `PR_READY`.
3. Binds an exact PID + context handle + detached user-root to a finite page
   lease. The page manager itself does **not** allocate/free system RAM.
4. Binds the PID to the actual selected session with parent-session checks.
5. Allocates an actual process-owned window, outside the desktop shell.
6. Commits one in-memory `nla_record` and returns PID + window ID +
   generation-checked lease ticket. This is not CPU activation.

A failure at steps 3–5 terminates the unrun `PR_READY` process, reaps stale
windows and user owner bindings, invalidates and wipes a bound user-root, and
reaps the kernel PID. The PID is **never reused**. Already-attested user pages
remain owned by their independent memory authority: no silent free/reuse.
If rollback cannot complete, `NLA_ROLLBACK_FAILED` signals manual recovery;
no implicit new attempt may be made. Retiring a bound frame lease is
**fail-closed** — that slot requires fresh memory preparation to be reused.

`nla_finalize` requires an already-terminal `PR_EXITED/PR_FAULTED`, a trusted
handle, a kernel-observed safe root, no scheduler dispatch running, and
**independent architecture evidence** that user execution, pending traps and
other CPU references have ended. It reaps user-owned windows, invalidates the
user root before reaping the PID and clears its transaction record.

## Verified by host sanitizer tests

- Two separate **real registry PIDs** tied to two user sessions and
  compositor-independent windows.
- Session selection locks the other user's windows; shell/compositor restart
  does not erase the original PID/session/lease state.
- Completion by fault and exit; revoked root is wiped; no orphaned PID/window.
- Denial for wrong current CR3, stale handles, unselected sessions, poisoned
  lease-generation overflow, foreign-parent PID, full window registry.
- On failure, new output is not written, no live kernel PID remains and a
  previously bound root remains revoked rather than silently recycled.
- Compiler test of kernel module in `x86_64-pc-win32-coff`; ordinary desktop
  link includes the dormant module without any calls from the shell.

Run: `python3 tools/test_native_launch_stage.py --output build/native-launch-host`.

CI also verifies the existing complete bootable ISO and the older QEMU desktop,
CPL3/EL0, paging, memory and shell regressions. A CI PASS proves only those
specified facts, not that a genuine ELF app has been run in production Desktop.

## Outstanding before native `.tea` process is functional

1. Secure, complete OWNED x86-64 page tables and runtime memory authority, not
   merely structural grants backed by fixed pages in a test PE.
2. Trusted `.tea` package validation in guest, executable ELF64 loader and
   initial CPL3 context (TSS/IDT/entry path/stack), plus isolation faults.
3. Kernel-owned scheduler lock and interrupt/preemption safety; real CPU time,
   resident page accounting, exit notification and cleanup across failure.
4. Bounded, copying syscall/IPC broker: no user pointers passed into these
   kernel APIs. Application window surfaces and input routing outside shell.
5. Authenticated session broker, filesystem ACL and user-home isolation; after
   that wire the user chooser to the approved design mockups.
6. Actual native `Processes.tea` program with Process / Performance / Details
   tabs, real PID and validated CPU/RAM, system-process protection.

Desktop v766 visual appearance and the F5/F8/F10 legacy UI sessions are
unchanged; **legacy UI 'PID' remains separate** from the real kernel PID. No
`main` update, no automatic migration or boot image replacement.
