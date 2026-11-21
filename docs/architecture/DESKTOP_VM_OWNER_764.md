# StyleOS 764 — admission of image-backed x86-64 page-table memory

Base: accepted **763**, commit `e7258f11607c271a1e842182697d3543a541bc78`.
Working branch: `feature/styleos-owned-arena-admission-764`.

## What was implemented

`desktop_vm_owner.c` is a *kernel-only, dormant* admission layer for the
fixed three-root DVM builder from 762. A trusted platform must first provide
an **exclusive, linker-declared, 4-KiB-aligned** image-backed arena of exactly
**78 pages**, including 72 pages for three table trees and six private user
code/stack/data pages. This stage does not allocate those pages or assume that
an arbitrary free RAM page has become owned by the kernel.

The admission stage verifies, before touching the physical table storage:

- The arena's address is exactly the address of the caller-supplied storage.
  The model is restricted to x86-64 UEFI identity-mapped image storage.
- The entire arena is within the image's existing boot-memory reservation.
  This memory-map check alone is *not* ownership of the arena.
- No overlap with live boot stack/handoff/source, memory-diagnostic workspace,
  or the existing dedicated boot pool and its metadata.
- Every user physical code/stack/data page occupies its **assigned** slot
  after the 72 table pages, with no caller-selected physical addresses.
- Supervisor mappings cannot alias the reserved arena. The DVM builder then
  independently rejects W+X, user/kernel permission leakage, bad VA ranges,
  and cross-root aliasing.
- A **mandatory read-only, trusted inherited-CR3 witness** must prove each
  page is actually identity-translated, writable, supervisor-only and WB.
  If the witness is missing or rejects any page, no table storage is written,
  and the output roots are invalidated. No default/always-true witness exists.
- Once every prerequisite passes, the original DVM builder constructs the
  three inactive page-table roots. On any builder failure, all table storage
  and all published roots are wiped.

The C-level host test uses a clearly **mocked** old-root witness and mocked
boot-memory evidence to test the fail-closed logic. Host PASS alone is **not**
a claim that production desktop page ownership has been proven. The native
Desktop currently has **no installed trusted witness** and does not invoke
this function at boot. These protections deliberately block user page
creation until the actual x86-64 firmware-inherited read-only page walker
and exclusive linker-owned arena are wired in and verified under QEMU.

## Explicit limitations

This is *not* a PMM, dynamic page allocator, online page table, verified map
of all PE/stack/IDT/GDT/TSS/framebuffer/MMIO dependencies, or running `.tea`
application. It has no `mov CR3` or CPL3 transition and introduces no PIDs.
It does not establish absence of hidden aliases to the image-backed user
pages until the desktop kernel dependency-closure audit is completed. The
kernel is still running on the original firmware-inherited root, and any
attempt to launch a native user app from this partial facility is prohibited.

## Test commands

```sh
python3 tools/test_desktop_vm_stage.py --output build/762-model
python3 tools/test_desktop_vm_owner.py --output build/764-owner
python3 tools/build.py --arch x86_64 --desktop-v0 --output build/764-efi
```

All existing UEFI/QEMU, F5/F10, dual-disk and CPL3/EL0 regressions remain
in the workflow. A final PASS must be taken from the CI run for the exact
764 commit; successful host compile and QEMU boot of the *unchanged* desktop
only prove non-regression, not activation of the new DVM roots.

## Next rigorous proof

Introduce a dedicated, linker-exclusive image arena in a **test image**;
import a trusted read-only old-CR3 identity+RW+U/S+PAT witness and prove
its operation against UEFI in actual QEMU. Admit and build the 762 roots only
after that proof. Independently enumerate/attest kernel image/stack/IDT/GDT/
TSS/device closure before any experimental switch to a 762 root.
