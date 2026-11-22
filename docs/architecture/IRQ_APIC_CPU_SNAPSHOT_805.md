# IRQ/APIC post-UEFI CPU-state snapshot — stage 805

## Scope

Separate `--test-irq-apic-805` terminal QEMU/OVMF-only EFI observes CPU features,
`IA32_APIC_BASE`, IDTR, RFLAGS/IF, legacy PIC IMR registers, and inherited CR3.
It executes **after** `ExitBootServices` and the ordinary EFI path's `cli`.
It does not write to LAPIC/IOAPIC registers, IDT, PIC masks, MSI settings or
pci configuration. No IRQ is fired or delivered. No topology is inferred.

The pure `irq805_validate` routine refuses missing APIC feature, disabled
APIC global-enable, impossible x2APIC mode, enabled IF, absent IDT, missing
CR3 or invalid local-APIC base. It does not claim local APIC MMIO access or
I/O APIC routing. Synthetic host probes are not hardware evidence.

An independent Python UART validator checks exact records and QEMU exit 33,
rejects extra lines, contradictory observed flags, fake policy claims, and
unverifiable observations. The real hardware test fails if QEMU/OVMF are absent.

## Limitations

- x86-64 Q35/OVMF only; the LAPIC base expected by the validator is
  `0xfee00000` for this fixed test hardware. Other machines not certified.
- `IA32_APIC_BASE` presence and IDTR do **not** prove installed usable
  exception handlers, MADT-derived IRQ/GSI routing, enabled IOAPIC redirection,
  mask correctness, external IRQ injection/delivery or IPI behavior.
- PIC masks are observed, not modified or assumed to be a configured IRQ plan.
- DMA/IOMMU, firmware runtime, all MMIO aliases and production CR3 remain
  unverified. `production_cr3_authorized` is unconditionally **false**.
- Ordinary desktop image must remain unchanged and omit test objects.

## Next dependency

Stage 806 should acquire and validate ACPI RSDP/XSDT/MADT before firmware
handoff and independently prove IOAPIC/IRQ override topology. Subsequent stages
should test real interrupt delivery with safe handler/vector boundaries,
DMA/IOMMU and UEFI runtime lifecycle. Only then approach kernel-owned production
CR3 and ordinary Desktop, `.tea` application and file-operation integration.
