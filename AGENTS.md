# StyleOS — engineering rules

The owner authorizes T-P1.0 to replace the MATRYOSHKA working tree, update main
after verification, and archive its original commit. These instructions govern
StyleOS; historical MATRYOSHKA instructions belong to Portable-os.

- Read README.md and docs/adr/ before changing boot or hardware contracts.
- Build for real x86-64/AArch64 instruction sets. Never reuse M32/M8 emulation as
  a kernel or claim a boot demonstration is a complete OS.
- Keep common code in kernel/ and include/styleos/, platform adapters in boot/
  and arch/. kernel_main() and versioned boot_info form the prototype boundary.
- Kernel model (microkernel, hybrid, monolithic), production ABI, memory manager,
  drivers, application formats and compatibility remain owner decisions.
- Do not modify LPYURA2/Portable-os. Preserve Git history; never force push.
- Use a task branch. T-P1.0 explicitly permits a verified fast-forward of main;
  later work requires a separate task. Do not create releases automatically.
- No secrets, personal disk images, firmware downloads or binary build outputs
  in Git. Selected text evidence belongs in docs/test-results/. Build tools may
  write only their declared output directories.
- Run the documented host checks, both builds, reproducibility and available
  QEMU smoke/negative checks. Record logs and PASS/FAIL/SKIPPED honestly. QEMU
  success does not establish VirtualBox, Android-host or physical-device support.
- Keep diagnostics usable before a scheduler/allocator exists. Handle invalid
  handoffs and UEFI failures explicitly; do not silently continue after errors.
- Explain important source files and the limits of each result in documentation.
- T-P1.1 is approved in its task branch only as diagnostic physical memory
  discovery. Do not change boot_info v1, allocate or reclaim pages, change
  translation/exception handling, or select a kernel model in this stage.
- Leave main unchanged. Record the owner's Termux run separately from Linux QEMU
  and stop before implementing a memory allocator or another architecture stage.

## Approved T-P1.2 task
- Branch feature/styleos-t-p1-2 starts from e4823f2. Publish this task branch only;
  do not update main, merge, release, force push, or modify Portable-os.
- Add terminal early exception diagnostics after own vectors/IDT installation.
  Preserve boot_info v1 and the T-P1.1 memory policy, MMU and allocator state.
- Keep synchronous syndromes distinct from unsupported asynchronous paths, real
  x86 hardware error codes distinct from absent fields, and validate saved PC
  against the injected instruction including actual PE relocation.
- Keep fault injection out of ordinary images. Preserve all T-P1.1 checks and
  the OVMF-independent ARM64 runner. Record phone results separately.
- No recovery, broken-stack/UART guarantees, IRQ enabling or kernel-model choice.
  Stop after the task branch and its factual report are delivered.

## Approved T-P1.3 task
- Branch feature/styleos-t-p1-3 starts from accepted 07fe425a0d4d5c0b9fbb65f8a37b30924523ec81.
  Publish this branch only; do not update main, merge, release, force push, or
  modify Portable-os. Preserve parallel work and all historical test logs.
- This task narrowly permits one fixed image-owned pool: eight 4096-byte pages
  in BSS, with separate 4096-byte metadata. It does not permit general physical
  allocation, heap, candidate RAM use, boot-memory reclamation or an ownership
  transfer of general RAM. Historical no-allocation stage rules remain in force
  outside this explicit image-buffer scope.
- Admit the complete pool and metadata intervals only after valid handoff and
  successful diagnostic map. Prove alignment, arithmetic, loaded/reserved image
  containment and exclusions for stack, handoff, source map/DTB and workspace.
  Reuse read-only internal diagnostic evidence without changing boot_info v1,
  map values/normalization, MMU, GDT, IDT, VBAR or exception protocol.
- Return mapped kernel addresses from this known array, never claim a generic
  C-pointer-to-physical translation. Prove full ARM ELF/BSS/Image coverage and
  full PE sections plus actual relocated placement, not just a start address.
- Keep one-CPU/page-at-a-time semantics, zero on every allocation, clear errors
  and unchanged allocation state on refusal. Document reuse/UAF and SMP limits.
- Place write/read selftest in a separate --test-bootpool image/output. Reject
  mixing with negative or exception modes; ordinary images do not run it.
- Preserve T-P1.2 regressions, exception PC proofs and four-artifact repro. Add
  host refusal/zero/reuse tests, strict binary/UART validation and real pool runs
  on x86-64, ARM initial EL1 and EL2. Keep ARM64-only route independent of OVMF
  and x86 tools. Record actual counts and logs; phone T-P1.3 is SKIPPED until run.
- Stop after this task's evidence and branch publication. General PMM, memory
  ownership, own translation tables and the kernel model need owner decisions.
