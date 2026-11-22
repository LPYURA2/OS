# StyleOS — stage 807 handoff

- Base accepted stage 806 commit: `1453f9397942f6358d0aad15cdb9a791c0b3c71f`.
- Branch: `feature/styleos-irq-delivery-807` (new stage, no main merge).
- Read `docs/architecture/REAL_IRQ_DELIVERY_807.md`, `AGENTS.md`, `docs/PROJECT.md`, `docs/design/APPROVED_UI_REFERENCES.md`.
- New isolated EFI `--test-irq-delivery-807` must prove real PIT0 IRQ0 via MADT ISA override to IOAPIC GSI2, Local APIC vector 0x50, ISR and EOI in QEMU/OVMF; no ordinary Desktop modifications.
- Strict negative verifier and host topology model required, real UART from same successful GitHub CI, archive source SHA/ISO.
- Do not claim PASS without final exact commit/CI evidence. Not a production IRQ implementation, PIT not restored as terminal EFI does not return.
- Next S808: DMA/IOMMU discovery and fail-closed requirements, or UEFI runtime, then own CR3 integration and end-to-end normal Desktop. Foundation not finished until integration passes.
- Keep approved UI and work order: foundation → Processes (real PID/CPU/RAM) → independent .tea → login/lock → StyleFS/Explorer → app store/network/browser/audio/VPN.
