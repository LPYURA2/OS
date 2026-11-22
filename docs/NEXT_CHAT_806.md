# StyleOS — stage 806 handoff

- Base accepted stage 805 commit: `a04be583e86acde0c9ddcc7f957b11cb33584e92`.
- New branch: `feature/styleos-acpi-madt-topology-806`.
- Isolated ACPI parser + test EFI: `kernel/acpi_madt_806.c`, `boot/x86_64/acpi_madt_806.c`, `include/styleos/acpi_madt_806.h`.
- `--test-acpi-madt-806`: only isolated x86-64 terminal profile. Observes firmware ACPI 2.0 config table **before ExitBootServices**, prints verified topology **after ExitBootServices**.
- Tests: `tests/test_acpi_madt_806.c`, `tools/acpi_806_verify.py`, `tools/test_acpi_806_negative.py`, `tools/test_acpi_806.py`; three new required GitHub Actions steps, including real QEMU/OVMF, and original UART evidence in CI artifact.
- Never claim PASS without checking full run, exact head commit and original UART. Report observed IOAPIC/GSI/ISA overrides as recorded, not guesses.
- IMPORTANT: ACPI checksum verification and IOAPIC discovery DO NOT mean any IRQ was routed, delivered, acknowledged or enabled. DMA/IOMMU, UEFI runtime, full own CR3 and regular Desktop integration are still incomplete.
- Stage 807: isolated *actual* IRQ delivery test with ACPI topology prerequisite. No `main` changes, no Desktop changes. Tell owner only when foundation genuinely completes in the regular OS.
