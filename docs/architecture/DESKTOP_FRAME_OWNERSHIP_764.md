# StyleOS 764 — bounded physical-frame claim ledger and safe revocation

Date: 21 November 2026. Branch: `feature/styleos-frame-ownership-764`.
Accepted input: Stage 763, `e7258f11607c271a1e842182697d3543a541bc78`,
[GitHub CI PASS](https://github.com/LPYURA2/OS/actions/runs/44901883843).

## Delivered

- A **kernel-only finite frame ownership ledger** (`desktop_frame_owner.[ch]`)
  over *externally admitted* physical page grants: 72 page-table frames and
  six unique user frames (code, stack, data for two future processes).
- Structural physical overlap checks; the 762 table builder then applies
  W^X, supervisor/user boundaries, private physical frames, same-VA/different
  PA support, finite roots and all-or-nothing zeroization on rejected plans.
- Once a native user PID has independently been admitted to `pr_table`,
  `dfo_bind` accepts it **only** with its exact recorded scheduler handle,
  root and READY state. No PID is created or inferred from a window session.
- A generation-stamped ownership ticket prevents stale retire requests.
  A root may be revoked only after the exact kernel PID has reached EXITED or
  FAULTED and after the privileged arch layer reports it is no longer active
  on the current CPU. Revoke zeroes **all** page-table pages of that user root
  and invalidates the published root. Duplicate or premature requests fail.
- The SAFE root may be cleared only after *both* contexts were revoked and it
  is not the active CR3. This step does not reclaim or scrub the six user-data
  physical frames: their lifetime is the upstream authority's responsibility.
- The ordinary UEFI Desktop now **links** the dormant module. No active
  desktop CR3 switch or change to the desktop renderer, apps, .tea format,
  dual-volume storage, source-recovery branch or `main`.

## Critical proof boundary

This module is a **custody/lease ledger**, *not* a physical-memory allocator,
physical ownership attester or production address-space switcher. A C caller
can describe arbitrary numeric PAs; the ledger verifies ranges and reuse,
**not** that firmware or the loader granted those pages. There is deliberately
no `booleans that make addresses trusted`, user-mode entry point, syscall,
candidate-RAM page reuse, or fictitious CPU/RAM telemetry.

**Do not call `dfo_prepare` in ordinary desktop** until a separate privileged
memory authority has proven: (1) real PA↔VA correspondence by verified UEFI
or page-table evidence, (2) ownership/reservation and exclusivity of the
entire arena and each user frame, (3) complete PE executable, stack, GDT,
IDT/TSS, framebuffer, MMIO and trap closure for every candidate root, and
(4) a safe allocation-and-release policy. The trusted caller must not accept
claims or process handles from an app. The existing 763 CR3 roundtrip used
other image-owned paging tables, **not** the 762/764 candidate roots.

The `observed_cr3` check is intentionally a necessary, **not sufficient**,
condition: single-CPU, no pending exception, disabled interrupts and root
lifetime must be proven by the arch backend. This test does not demonstrate
physical page scrubbing, real multi-process scheduling or launching `.tea`.

## Checks

```sh
python3 tools/test_desktop_frame_owner.py --output build/frame-owner-host
python3 tools/test_desktop_vm_stage.py --output build/dvm-host
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/764-efi
```

Host tests use ASAN+UBSAN for physical alias refusal, W^X and rights inherited
from 762, exact PID and scheduler-handle binding, stale generation rejection,
terminal-state guard, active-root guard, scrubbed-root revocation, all-or-
nothing rollback and double-release refusal. Freestanding x86 COFF build
ensures linkage. Full QEMU/OVMF, real x86/ARM CPL3/EL0 and prior Desktop
regressions must pass *in CI*; do not infer guest success from host PASS.

## Next stage 765

Implement a privileged **physical grant attester** for the desktop x86-64
UEFI environment, with a read-only walk of the inherited mapping and exact
image-owner/firmware-map proof (including exclusions). Use an independent
QEMU-only profile to verify owned frame backing and complete kernel closure.
Only **then** feed genuine grants into this 764 ledger and activate candidate
roots in a dedicated test; production desktop and `.tea` stay behind those
acceptance gates. A separate later phase should add login/profile sessions
and the approved login screen mockup, rather than making the screen imply
unimplemented access isolation.
