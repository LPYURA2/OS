# T-P1.2 — independent x86 exception review

Status: **PASS, no blocking correctness findings in the reviewed x86 sources.**

Scope: read-only source review of `arch/x86_64/entry.S`, `exceptions.c/.h`, `exception_entry.S`, the common exception declarations/formatter and surrounding UEFI entry. No repository edits, builds, tests, QEMU runs, commits, pushes or Portable-os changes were performed by this reviewer. Root is responsible for integrated runtime acceptance.

## Checked properties

- UEFI calls `styleos_enter_kernel` only after successful ExitBootServices. Entry masks IF, clears DF, selects the image-owned stack, aligns it to 16, reserves 48 bytes, saves boot_info at caller RSP+32 and installs IDT before entering common kernel code. That save lies outside the callee's 32-byte Microsoft x64 shadow space and remains intact across the IDT-install C call.
- IDT is image-owned, aligned, has 256 16-byte gates, DPL0 present interrupt-gate attributes 0x8e and IST=0. The loaded IDTR has the correct 10-byte packed shape and 4095-byte limit.
- The active CS is read instead of assuming a firmware-specific constant; null selectors, RPL != 0 and TI/LDT selectors are rejected. The code intentionally inherits the existing 64-bit ring-0 GDT entry. It does not install a private GDT/TSS, configure IST, reclaim the firmware GDT's memory or promise broken-stack/double-fault recovery. That dependency must remain explicit in the final documentation.
- Stub and address-table `.irp` lists each contain exactly vectors 0..255. The hardware-error set is correct: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30. Other vectors get synthetic padding plus an independent has_error=0 flag.
- Both frame classes normalize to vector at +0, has_error +8, error/padding +16 and saved RIP +24. The handler reads these values before changing RSP. No assumption about optional SS/RSP tail fields is needed to read RIP at these offsets.
- #UD vector 6 uses has_error=0; common formatting keeps error absent even if a mistaken caller passes a truthy has_error for a vector without an architectural error code. A synthetic zero is not displayed as a hardware #UD error code.
- CR2 is read by the assembly only when vector == 14. Other vector paths pass zero and common formatting displays cr2=absent.
- Handler uses CLI and CLD, realigns interrupted RSP down to 16 and reserves 48 bytes. Arguments 1..4 use RCX/RDX/R8/R9; argument 5 is at caller RSP+32, i.e. callee RSP+40 after CALL. Shadow space and call alignment satisfy Microsoft x64 ABI independently of interrupted RSP alignment. The handler is terminal and does not IRET or restore the interrupted context.
- Shared C entry claims the first exception before formatting; a later entry halts before another UART/formatter pass. This is a bounded synchronous-diagnostic guard on a single boot CPU, not an atomic asynchronous NMI recovery mechanism.
- COM1 writes remain bounded by finite polling. If UART never becomes ready, each attempted character eventually returns and the terminal path reaches halt; serial output itself is not guaranteed for a failed UART.
- Test fault is under STYLEOS_TEST_EXCEPTION only. `styleos_test_fault` begins directly with UD2, with no prologue. Its runtime address emitted by C contains the actual PE relocation; final testing must independently relate that address to the fault RVA and PE image runtime base, rather than preferred ImageBase.

## Acceptance boundaries

This source review does not establish that IDT delivery or the PE-relative test PC worked in QEMU; root's integrated normal/exception logs and independent artifact-PC check must supply that evidence. Unchanged boot_info/memory-policy checks and absence of new emergency markers in normal/old negative scenarios remain parent-review scope. No x86 source blocker was found that requires a code change.

Source SHA-256 snapshot and independent list-completeness check: `x86-review-hashes.json` beside this report. Re-review is required if the four x86 files change after this snapshot.
