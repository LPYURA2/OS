# Stage 789 — terminal PML4 slot pruning under hardware nested fault

Task branch `feature/styleos-pml4-root-pruning-789` from the accepted 788 commit
`7e3b7d38c9a60f58a1094b27d9ca111df7518962`.

## Narrow change

Dedicated `--test-root-slots-789` x86-64 QEMU/OVMF EFI only. The normal
Desktop and all earlier profiles retain their code paths and build artifacts.
Stage 788 demonstrated a real #DF on IST3 under a root which inherited every
UEFI PML4 entry except entry 128. Stage 789 prunes the *new* experimental
PML4 to exactly **entry 0 from the old root** and **image-owned entry 128**,
with no other populated entries. Old root, original firmware maps, and accepted
stages are never written. This is terminal: never return to UEFI or attempt to
restore TR with LTR(0). Hardware errors, resets and timeouts must fail.

`r789_prune_inherited_root` rejects root overlap, missing inherited low slot,
occupied alias, altered staging recipe, an invalid/missing required address, or
any required execution/data/stack/console address outside the first 512GiB.
Failures leave the staging root unchanged. Dedicated host test uses ASAN/UBSAN,
negative cases, and a freestanding COFF compile.

Before activation the test validates relevant low-address GDT and IDT pointers,
keeps verified image-owned stack and descriptor frames, and admits a true
hardware CR3 transition only after the pruning audit. The terminal sequence
reuses 788 nested real `INT3 -> IST1 -> #PF guard -> unavailable #PF gate -> #DF IST3`.
Live #DF handler reads back the active PML4, verifies every other
entry exactly zero and matching PML4[0], the IST entry PML4[128], TR, CR2,
CPU error code and frame. The CPU may set Accessed (bit 5) on PML4 entries after CR3; only that
architecturally permitted mutation is accepted in the live readback. UART
carries measured root entries (not only success
markers); QEMU exit must be 33. Negative parser tests reject changes to entry
values, slot count, RIP, stack or exit code. No synthetic UART can establish
hardware acceptance.

## Important remaining dependencies

**This does not create a standalone low-half page hierarchy.** PML4 entry 0
still points to inherited firmware PDPT/PD/PT structures; their permissions
and execution attributes remain inherited. The retained low-address region
can still expose firmware mappings, device dependencies, RWX permissions, and
other addresses not yet explicitly audited. This does not prove kernel-wide
W^X, true kernel/user process isolation, production #DF recovery, multi-core
safety, autonomous `.tea` execution or readiness to enable it in Desktop.

Next stage should inventory and independently own a complete minimal low-half
closure (code RX, data RW/NX, stacks RW/NX, descriptors RO/RW as required,
MMIO as needed). Demonstrate hardware with a true detached slot-0 hierarchy
and fail-closed permission checks before any runtime Desktop integration.

## Acceptance

Exact successful GitHub Actions for final SHA, `root-slot-789-host/test.log`,
`root-slot-789-parser.log`, real `root-slot-789-qemu/serial.log`, `results.json`,
QEMU exit 33, source ZIP, ISO and evidence artifact, Desktop unchanged. No
assumption of PASS from compilation, snapshots or a positive fixture.
