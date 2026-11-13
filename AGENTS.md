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
