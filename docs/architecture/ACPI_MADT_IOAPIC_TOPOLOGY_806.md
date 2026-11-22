# StyleOS stage 806 — ACPI MADT and IOAPIC topology (read-only)

Stage 805 proved a post-EBS Local APIC/IDTR/PIC CPU snapshot only; it did not prove an interrupt route or delivery. Stage 806 uses the UEFI ACPI 2.0 configuration-table GUID **before** ExitBootServices, validates the RSDP/XSDT/MADT signatures, version, bounds and ACPI checksums, and copies a bounded summary to image-owned memory. After EBS it reads no firmware APIs or ACPI pointers; it prints its summary to COM1 and enters the dedicated terminal QEMU debug-exit.

## Checks / provenance
- ACPI 2+ RSDP, 36-byte structure; checksum for first 20 bytes and full 36 bytes; 64-bit XSDT pointer and XSDT checksum.
- XSDT must have a valid aligned pointer list of at most 128 distinct entries; exactly one bounded and checksummed MADT must be found.
- MADT: aligned Local APIC PA, enabled LAPIC/x2APIC count; no more than eight IOAPIC records; distinct IOAPIC IDs, base GSIs and physical MMIO addresses; up to 16 ISA IRQ overrides with valid polarity/trigger encoding and unique IRQ sources. All entries require positive lengths and exact known type sizes.
- Pure host model tests run with ASAN/UBSAN and COFF; strict UART verifier rejects false IRQ-delivery claims, spoofed PASS, duplicate rows, count mismatches and malformed fields.
- Required **real** QEMU/OVMF q35 run captures `serial.log`, `results.json`, QEMU debug-exit 33 and binary SHA-256. No fallback to synthetic hardware logs. The accepted source and ISO come from the same successful GitHub CI.

## Explicit gaps
- This is a QEMU OVMF read-only **firmware topology observation**, not device driver installation or an IRQ routing demonstration.
- No MMIO IOAPIC redirection table readback, no IRQ unmask, no IDT update, no delivery via PIT/HPET/APIC timer, no EOI handling.
- Firmware pointers are trusted QEMU/OVMF interfaces and not yet proven against a full UEFI physical-memory ownership map. This isolated test is **not** a general-purpose safe ACPI parser for arbitrary physical hardware.
- No DMA/IOMMU, runtime-service closure, global MMIO alias proof or production CR3 integration; `production_cr3_authorized=false`.
- Production Desktop remains unchanged, identical in build inputs to stage 805. No merge to `main`.

Next stage 807: plan and prove actual IRQ delivery in isolated hardware mode (MADT IOAPIC routing / IDT / EOI) with explicit bounded timeouts and failure evidence, without enabling interrupts in the ordinary Desktop.
