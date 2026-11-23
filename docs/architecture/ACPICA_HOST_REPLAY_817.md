# StyleOS 817 — ACPICA host-side AML replay

Base: accepted 816 SHA 99e8d873aba5725e77760d0360280763c0d14fb9.
Branch: feature/styleos-acpica-replay-817.

## Scope

This is the first use of an established AML interpreter (ACPICA AcpiExec).
It executes *outside* StyleOS on an unprivileged Ubuntu CI host against
byte-verified real QEMU/OVMF DSDT and optional SSDT captured by a fresh,
unchanged 816 diagnostic. This is **not** a kernel ACPICA port or a physical
ACPI resource ownership proof.

The known synthetic DSDT method X817 returns 0x817 and an SSDT method
S817 returns 0x5a5; AcpiExec must execute both with matching return output.
The real QEMU DSDT is replayed under AcpiExec for _SEG, _BBN and _CRS probes.
The external simulator cannot establish what these methods would read from
physical OperationRegions or the motherboard resource reservations.

## Safety boundary

- acpiexec runs without shell, under CPU, RAM, core and file-size limits.
- -di prevents _STA/_INI init execution and -do disables simulation of
  OperationRegion addresses. This is not sufficient to *certify* a safe
  production AML interpreter.
- No PCI/ECAM/IOMMU hardware handles or permissions are passed to AcpiExec.
- Model tests reject malformed DSDT/SSDT checksums, forged guest UART,
  unverified QEMU exit, falsified ACPICA return and synthetic success text.
- All S817 authorization flags remain false: PCI ownership, ECAM reservation,
  DMA domains, isolation, deny/allow, production CR3 and UEFI runtime.
- No original boot, desktop, processes, .tea or StyleFS code is changed.

## CI / evidence

Ubuntu 24.04 installs acpica-tools; AcpiExec executable SHA and help/version,
iasl SHA, input firmware tables, synthetic AML and logs, original QEMU
UART/JSON, negative test outputs, exact source ZIP and unchanged ordinary ISO
are retained by GitHub Actions. Real hardware is QEMU/OVMF only. Termux,
VirtualBox and physical PC remain NOT RUN.

Commands:

    python3 tools/test_acpica_replay_817_negative.py
    python3 tools/test_acpica_replay_817.py --output build/recovery-canonical/acpica-817

Do not mark this stage accepted until exact-head full CI SUCCESS and independent
archive review. Real _CRS/_SEG/_BBN hardware evaluation is NOT verified
even if AcpiExec reports a simulated object.

## Next dependent steps

Port and audit an ACPICA OS services layer in a separate experimental branch,
with allocator, locks, scheduling, runtime method limits, namespace overlay,
deny-by-default OperationRegion, and measured safe resource policy. Then
prove ACPI host bridge _CRS/_SEG/_BBN + MCFG/ECAM, real IOMMU requester
ownership, aliases/exceptions, hardware DMA deny/allow/fault/invalidation.
After UEFI runtime and own production CR3, run the integrated
kernel+Desktop+Processes+native .tea+StyleFS acceptance. Only after that
may the StyleOS foundation be declared complete.

**StyleOS foundation NOT COMPLETE.**
