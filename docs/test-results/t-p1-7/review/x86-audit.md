# T-P1.7 independent x86-64 static audit

PASS — no open x86 acceptance blocker found in the reviewed implementation.

This was a read-only source audit. No repository edits, tests, QEMU runs,
Git mutations or remote operations were performed. The parent owns integrated
builds, actual hardware logs, linked-code proof, final evidence and publication.
Static audit alone does not establish a machine-run PASS.

## Scope and findings

- The inherited-root admission is bounded and read-only before new-storage
  writes: every visited physical table page is RESERVED and non-CANDIDATE
  before dereference, the complete image is identity/WB, mutable new objects
  are writable under the inherited root, all new resources and exclusions
  are checked as whole intervals, and the visited-table identity closure is
  completed. The ADR-0007 UEFI physical-access assumption remains explicit;
  this is not a generic pointer-to-PA interface.
- Three independent finite 32-page arenas have only 4-KiB leaves. SAFE has no
  user leaves; A and B each have exactly their own RX code, RW/NX stack and
  RW/NX data mapping. Full image aliases remain supervisor-only. New mappings
  are non-global, PCID/LA57 and unsupported privilege extensions are refused,
  and initial PGE flushing removes inherited global entries before own roots.
- The private data B VA is unmapped under A. The known A load therefore has
  the intended user/read/not-present #PF expectation (error 4), with CR2 and
  hardware saved RIP captured rather than inferred from a log marker.
- GDT descriptors encode kernel CS 0x08/SS 0x10, user CS 0x23/SS 0x1b and the
  64-bit TSS at TR 0x28. TSS.RSP0 points at the separate admitted 16-KiB entry
  stack; the I/O bitmap is outside the TSS limit with IOPL zero. The IDT has
  DPL3 only for int 0x80; #PF uses the dedicated real hardware entry and other
  exceptions remain terminal diagnostics.
- The previous LGDT-to-LIDT diagnostic gap is addressed by
  inherited_idt_compatible() and a temporary own-GDT alias at the actual old
  CS selector. Unsupported selector collisions are refused while old state
  remains intact. The GDT limit is narrowed only after new IDT gates using
  CS 0x08 are active. The old GDT is not written.
- The enter ASM saves the original dispatcher's eight Microsoft-x64
  nonvolatile integer GPRs and aligned stack, switches to the selected root,
  restores all fifteen user GPRs and performs IRETQ with five explicit words.
  User SP is not reused as a privileged exception stack.
- Both real user gates normalize a 176-byte frame: fifteen captured GPRs,
  vector/error and hardware RIP/CS/RFLAGS/RSP/SS. The #PF error is not software
  padding. Raw user frames are aligned at 16; the C call has full 32-byte
  shadow space. On privileged-origin #PF, CS is examined before user RSP/SS
  are accessed and early_exception_x86 remains terminal.
- The full user integer context is copied from the raw frame; unused x86
  normalized words are zero. User mode/root/stack/operation checks are real
  hardware observations. The common validator adds exact PC/VA/syndrome,
  markers, stack cookies and deterministic scenario-phase checks.
- On return, ASM loads SAFE before discarding the entry frame, restores
  kernel SS/DS/ES, restores original dispatcher SP/GPRs and returns through
  the original C call. up_arch_run verifies actual SAFE and a hardware event,
  then invalidates context/event/selected-root/dispatcher-SP fields and the
  running slot before common slot release.
- B checks preserved volatile R10, nonvolatile R12, stack cookie and private
  data after resume, then writes DATA_AFTER before its real exit gate. A's
  fault PC is never an architectural exception return destination. The common
  engine must continue to enforce the four-event order and never select
  terminal A; that common logic is separate from this architecture audit.

## Limits

This is one-CPU integer-only cooperative code with two embedded known programs.
It does not establish general FP/SIMD context, arbitrary executable loading,
production syscall/process ABI, W^X for supervisor image aliases, IRQ/preemption,
SMP, kernel-fault recovery or general allocator ownership. No actual hardware
result is asserted by this static audit. Final machine evidence must bind raw
frames, live roots/descriptors and user instruction PCs to the linked image.

## Snapshot

Recorded UTC: 2026-11-16T16:16:20.507861+00:00

- `arch/x86_64/user_processes.c` SHA256 `114eeec1920d65d89f3ebc26adb8d010a0caca298311381910242d6a60ecea18`
- `arch/x86_64/user_processes_entry.S` SHA256 `4dd0842487cf46bd55b0a3089444253c85b59627369d7034fae29677a4452b9c`
- `include/styleos/user_processes.h` SHA256 `2874269f87565cb210f92d3c530cf1b4e7d708978e99f7e25dcc82de786d7cd4`
- `kernel/user_processes_demo.c` SHA256 `d73239463c3d3489cdaaa9ff9b67faa47fc5a8e2405ce5b00796e866170174a1`
