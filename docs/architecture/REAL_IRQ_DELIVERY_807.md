# StyleOS S807 — terminal real PIT->IOAPIC->Local APIC IRQ delivery

Built from accepted S806. Scope: isolated QEMU/OVMF only; **NOT** a production
interrupt subsystem. The EFI retrieves checked ACPI MADT data pre-EBS, then
uses that image-owned topology after EBS. The terminal profile sets an IDT
with a real ISR at vector `0x50`, programs the IOAPIC redirection entry for
**ISA IRQ0**, respecting MADT's IRQ0->GSI2 override, starts PIT channel 0
at ~100 Hz, enables IF for a bounded TSC interval, and expects the CPU to
enter the own ISR. The ISR verifies its LAPIC in-service register before and
after the EOI register write. Strict UART proof rejects a fake success record,
missing delivery, missing EOI or missing IOAPIC redirection and state restore.

Safety boundaries:
- terminal only; never included in ordinary Desktop/production image
- no PIC unmask; the original PIC masks must already be both `0xff`
- reject x2APIC and unsupported multiple IOAPIC/CPU configuration
- mask IOAPIC entry first, set destination, configure and verify readback;
  after CLI, restore original IOAPIC redirection, LAPIC TPR/SVR and IDTR
- PIT mode/count cannot be restored to the previous precise timer phase;
  terminal EFI **never returns to the OS** and records `pit-restored=no`
- the root MMIO identity mapping is inherited from OVMF and is not proof
  of the eventual owned StyleOS CR3
- if no IRQ arrives: fail, bounded time and QEMU timeout; never manufacture PASS
- all unhandled vectors terminate the virtual machine with a failure debug-exit
- no global DMA/IOMMU, firmware runtime, MSI/MSI-X, SMP, stable production IRQ
  routing, MMIO alias scan or own CR3. `production_cr3_authorized=false`.

Acceptance: independent ASAN/UBSAN COFF host model, host negative tests,
real QEMU/OVMF UART, exit 33, CI artifact with UART+SHA256, full inherited
regressions, unchanged ordinary Desktop and source/CI parity.
