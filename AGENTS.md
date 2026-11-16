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

## Approved T-P1.4 task
- Branch feature/styleos-t-p1-4 starts from accepted
  877f5e2bb85d484852a627e3a76a4bb956b40d50. Publish this branch only. Do not
  change main, previous remote refs, Portable-os, history or releases; no merge
  or force push. Preserve parallel work and historical logs.
- Only --test-paging and --test-paging-fault may switch translation tables.
  Both use separate outputs and are mutually exclusive with each other and
  all older test modes. Ordinary images and older profiles keep their previous
  x86 CR3 / ARM MMU path and contain no paging-test code, buffers or fault site.
- Keep boot_info v1, memory normalization, the eight-page boot pool and older
  exception protocols unchanged. Use only a finite image-owned table reserve,
  separate control/data pages, whole-span admission and explicit VA/PA proofs.
  Never use candidate RAM, reclaim boot memory or return active tables to a pool.
- Validate mode, widths, critical mappings, rights/type, capacity and an absent
  canonical probe before activation. Refusal must not publish a usable root.
  Preserve the current stack, exception path and console; no new GDT/TSS/IST.
- Use 4-KiB leaves for a bounded image closure plus only required GDT/PL011
  pages and one data alias. State the image RWX policy honestly; do not claim
  W^X, production isolation, arbitrary pointer translation or general RAM ownership.
- Retain the current GDT. Permit hardware Accessed-bit update only through a
  bounded RW/NX mapping of its active descriptor byte page when A=0; prove
  the whole PA page already reserved and outside CANDIDATE. Keep other GDT
  pages RO/NX, do not write descriptors in software or create a new GDT.
- Prove hardware root/mode readback, live per-page descriptors and alias RW.
  Fault images must produce one real #PF or EL1 Data Abort with precise
  read/translation syndrome, target VA and saved instruction PC bound to the
  binary. PASS-only, silence, timeout, reset and software imitation are failures.
- Preserve all older tests and four-artifact reproducibility. Add host refusal,
  binary/UART and six QEMU paging scenarios; retain the OVMF-independent ARM64
  route. Record actual counts and distinguish Linux from Android/Termux.
- Stop after evidence and task-branch publication. General PMM/heap, ownership,
  recovery, IRQ, SMP, scheduler, processes, IPC, user mode, drivers and kernel
  model remain outside this task and require a separate owner decision.

## Approved T-P1.5 task
- Branch `feature/styleos-t-p1-5` starts from
  `8bc7e78592c1aef78429042658f9ae02f71637c5`. Publish this branch only; preserve
  main, previous refs, Portable-os and history. No merge, force push or release.
- Add a read-only physical ownership plan only in `--test-ownership-plan` and
  `--test-ownership-plan-invalid`, with separate outputs. All eight test flags
  are mutually exclusive. Keep boot_info v1, normalization, pool, exceptions,
  paging profiles and the four ordinary reproducible artifacts unchanged.
- Run after valid handoff/MEMMAP on inherited x86 CR3 or ARM EL1 MMU off.
  No CR3/MMU/GDT/IDT/VBAR changes, IRQ, candidate-RAM probes/reads/writes,
  general allocation or reclaim. New persistent writes use only the admitted
  aligned 65536-byte image-owned BSS metadata workspace; C locals use the
  existing image-owned stack. Keep new code/buffers out of previous profiles.
- PROTECTED is the union of baseline policy reserves and outward-rounded live
  physical claims. REVIEW is the inward-rounded diagnostic candidate remainder,
  with no-transfer reason; outside coverage stays UNCLASSIFIED. Merge nested
  ranges without double counting. GRANTED is always zero; structural validity
  and even unresolved=0 do not prove an ownership transfer.
- Prove full image/BSS, stack, handoff, source map/DTB, diagnostic/plan workspace,
  pool/metadata, and active architectural dependencies. Protect loader/firmware,
  unknown UEFI types and FDT reservations by baseline policy even when they are
  not separately listed live objects. Pool is present but not admitted here;
  T-P1.4 paging buffers are not built in this profile.
- Permit read-only old-root x86 walks only after full table/GDT PA-page RESERVED
  and non-CANDIDATE admission, under the documented narrow UEFI access contract.
  Verify visited-table identity closure and real GDT translations; emit partial
  descriptor evidence without claiming all inherited tables or image leaves.
  Preserve unresolved flags for incomplete old tables and other firmware state.
  ARM EL1/EL2-to-EL1 remains MMU off with full Image/DTB/vectors and PL011 as MMIO.
- Reject invalid/missing/contradictory mandatory evidence before publishing any
  partial plan. The controlled invalid profile changes only its own IMAGE proof.
  Independently verify binary extents/SHA, UART sources/ranges/walks/mode and
  zero grants; a PASS string, timeout, reset or silence alone is insufficient.
- Preserve all old regressions; test new positive/refusal profiles on both ISAs,
  ARM initial EL1/EL2 and changed DTB memory sizes. Record actual counts, logs,
  tool versions and base/code/report SHA. Linux ARM-only is not Android PASS;
  T-P1.5 Termux, VirtualBox and physical runs stay SKIPPED until actually run.
- Stop after report and task-branch publication. The first general-RAM pool,
  PMM/heap, reclaim, own GDT, IRQ/SMP, recovery, processes/drivers and kernel model
  remain separate decisions. A plan with zero grants does not authorize them.

## Approved T-P1.6 task
- Branch `feature/styleos-t-p1-6` starts from
  `ea37e76abb0a1b6e75c050c43e357d8c89981537`, the accepted T-P1.5 branch tip.
  Publish this task branch only. Preserve main, previous refs, Portable-os,
  `docs/styleos-readme`, history and parallel work. No merge, force push or
  release. Do not modify the presentation README for this technical task.
- Only `--test-tasks` may include the two trusted cooperative kernel tasks,
  context adapters, two 16384-byte stacks and 4096-byte control workspace.
  Use separate outputs; all nine test flags are pairwise incompatible. Ordinary
  and previous profiles retain their sources, artifacts and architecture paths.
- Activate only after valid handoff and MEMMAP. Prove every complete image-owned
  BSS/control/stack interval, arithmetic, alignment, reservation and disjointness
  from all other live objects. Do not use CANDIDATE/REVIEW, heap, general PMM,
  pool allocation, reclaim or new physical ownership; GRANTED remains zero.
- One CPU, two fixed slots, explicit FREE/READY/RUNNING/DONE transitions and
  round robin at explicit calls only. Refuse invalid arguments, state, overlap,
  capacity and counter overflow without modifying control or output. DONE is
  never selected; a returning task must enter the bootstrap's controlled DONE
  path. Release clears slot bookkeeping only, without freeing stack RAM.
- Use real ABI call-boundary SP/PC and nonvolatile integer registers: Microsoft
  x64 with shadow space/no red zone and AAPCS64 with aligned SP. Preserve the
  original dispatcher stack; construct an explicit initial frame and return
  trap. The 16-byte software canaries at each stack end do not provide hardware
  stack protection, task isolation or recovery from a damaged kernel task.
- Keep the integer-only compiler profile and bind complete executable-byte
  decoding to the linked image using LLVM objdump, rejecting gaps, bad decode
  and FP/SIMD instructions. Do not claim general FP/SIMD context, unwind,
  preemption, IRQ, SMP, TLS, user mode, processes, syscalls or IPC support.
- Preserve inherited x86 CR3/GDT/IDT and ARM EL1 MMU-off/VBAR/DAIF. Read-only
  architecture observations require the known-image/reserved-table/GDT proofs;
  they do not create a general VA-to-PA translator or complete firmware closure.
- Require the exact A:START, B:START, A:RESUME, B:RESUME, A:EXIT, B:EXIT trace,
  six GO operations, two yields per task, return/DONE, dispatcher EMPTY/release,
  live register checks, saved continuation/stack evidence, canaries, one TASKS
  PASS and ordinary halt. Host logic tests do not prove machine switching.
- Preserve old regressions, four-artifact reproducibility and same-toolchain
  comparison of all 28 previous profile artifacts. Record actual counts and
  SHA-bound PE/ELF/Image, UART/JSON and manifest evidence. Keep the ARM64-only
  runner independent of OVMF, x86 QEMU and x86 linker; LLVM objdump is required
  for the new task proof on either ISA.
- Record the owner's T-P1.5 Termux 35/35 exit-0 QEMU 11.1.3 screenshot report
  separately in the T-P1.6 report, without altering historical evidence or
  treating it as a T-P1.6 result. T-P1.6 Android/Termux, VirtualBox and physical
  devices remain SKIPPED until actually run. Stop after report and publication.
  User-process fault containment belongs to the separate future T-P1.7; these
  cooperative kernel tasks do not establish it or implement Deep Drop.
