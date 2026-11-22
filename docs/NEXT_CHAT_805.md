# StyleOS — stage 805 handoff

- Accepted base: stage 804 SHA `adbac259a2103474837be1eab4e0a3ca31cf0906`.
- Branch: `feature/styleos-irq-apic-snapshot-805`.
- New isolated terminal EFI: `boot/x86_64/irq_apic_805.c`;
  pure policy module `kernel/irq_apic_805.c`;
  model tests and strict UART tests in `tests/` and `tools/`.
- Hardware requirement: run the `805 Observe post-EBS ...` step in true QEMU/OVMF,
  capture original `serial.log`, QEMU exit code 33, run host ASAN/UBSAN COFF,
  negative UART tests, inherited CI and source+ISO archive checks.
- Do not claim any CI result before checking the exact run/head SHA.
- 805 is APIC *state observation*, not functional IRQ routing.
  MADT, IOAPIC route, IRQ delivery, DMA/IOMMU, UEFI runtime, production
  CR3 and full native Desktop integration remain unverified.
- Stage 806: ACPI MADT / IOAPIC topology, conservative read-only probe first.
- Keep normal desktop and UI, `main`, accepted branches unchanged.
- Tell owner separately when foundation genuinely completes in the regular OS;
  diagnostic progress is not foundation completion.
