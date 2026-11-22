# Stage 786 — terminal CR3 + TSS/IDT IST1 hardware #BP entry

## Scope and invariants

Base: accepted 785 `50a39e8ec25f62bf9ac239842df718023bb38253`, Actions `45028051806` SUCCESS. Branch `feature/styleos-hardware-ist-entry-786`. No edits to main, accepted branches or ordinary Desktop. Stage 786 is a separate `--test-ist-hw-786` PE image. It is intentionally **terminal**: on pass/fail it exits QEMU via `isa-debug-exit` and never calls UEFI, resets TR, or IRETs.

Nine **new** aligned, image-owned frames: PML4, PDPT, PD, PT, IDT, GDT, TSS, RSP0, IST1. Each must independently pass the inherited-root actual-PA image identity + writable walk, and the entire image needs valid admission before any switch. No candidate RAM is granted/reclaimed. The new PML4 is a copy of the inherited root with one exclusively absent slot 128 filled. All other inherited mappings (including the currently executing image, stack, serial, old IDT, firmware) are retained; **no production kernel closure or W^X guarantee is claimed**. A clean new PML4 slot 128 is mandatory. Hardware NXE and CR0.WP must be present; LA57 must be disabled.

IST alias `0x0000400000003000`, exactly one P|RW|NX|supervisor leaf, guard neighbors `[alias-4096, alias)` and `[alias+4096,alias+8192)` with **exactly zero PTEs**. Once the CPU writes through the alias, the active page-table hierarchy
**must** have hardware Accessed (A) bits on PML4/PDPT/PD and A|Dirty (D)
on the IST leaf. These bits are permitted and explicitly required; all other
entry bits must match the staged layout. The two guard PTEs remain exactly zero.
The initial trial found an expected QEMU failure when the verifier incorrectly
required A/D to remain zero after real hardware accesses; it was corrected
in the same unaccepted 786 branch, with no alteration of inherited 785.

The physical IST page stays image-owned and also accessible by its existing inherited identity mapping; this is not production one-alias stack isolation. A volatile marker written through the alias after CR3 switch must be visible through the physical image address.

Fresh GDT includes an available long-mode TSS descriptor, new TSS has RSP0 in a separate image frame and IST1 pointing into the one mapped alias. Fresh 256-entry IDT duplicates inherited gates, replacing only vector 3 with a ring-0 interrupt gate set to IST=1 targeting a dedicated assembly stub. No other entry, ISR, ordinary Desktop or user ABI is modified.

## Terminal hardware acceptance

The CPU must actually execute `mov cr3`, `lgdt`, `ltr`, `lidt`, `int3`, and land in the standalone ISR3 on the IST1 stack. The stub immediately captures hardware RSP before aligning its Microsoft x64 call frame; the verifier reads the **hardware-pushed** RIP/CS/RFLAGS/previous-RSP fields, checks RIP against the instruction after the actual INT3 and old RSP inside the inherited admitted boot-stack span. It checks active CR3, TR, GDTR/IDTR, NXE, WP, IF=0, alias marker, guard PTEs=0 and the unique expected NX supervisor leaf. Only then does the handler emit the attested TRAP record and QEMU exit value 0x10 (host exit 33). Any miss/hang/fault/reset/wrong UART marker/exit code must fail.

**Explicitly not proven:** an intentionally faulted guard page, recovery from stack exhaustion, #DF, nested fault, IRQ/NMI delivery, SMP, production CR3 closure, W^X on inherited mappings, secure alias-free stack, any concurrent independent `.tea` or ordinary Desktop integration. Unexpected fault may still triple-fault and reset; do not misclassify it as PASS. The new root inherits platform/firmware mappings intentionally and is not a production independent root.

## Exact acceptance checklist

1. QEMU OVMF terminal `test_ist_hw_786_guest.py` PASS with `qemu_exit_code=33`, real UART and strict independent parser; `test_ist_hw_786_verify.py` negative tamper cases PASS. No synthetic QEMU success.
2. Existing real 784 LGDT/LTR test PASS and 785 host guard proof PASS; all inherited CI regressions PASS.
3. Ordinarily compiled Desktop EFI byte-identical to the accepted 785 CI EFI at the same toolchain. If deterministic comparison cannot be performed, record not verified — no false claim.
4. CI exact final commit PASS, source ZIP, ISO, evidence and handoff. No main, accepted branch or release mutation.
