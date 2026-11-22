# StyleOS 798 — PCI MMIO extent census, measured vs unknown

**Base:** accepted 797 `f07cf08c95da7175bac6dcf6cc7b03c5bb45022d`.
**Branch:** `feature/styleos-pci-mmio-extent-attestation-798`.

In the same GitHub Actions job, stages 795, 796 and 797 rerun their genuine
QEMU/OVMF tests. Stage 798 verifies SHA-256 of every original UART, exit=33,
restored BAR/command registers of S795, and strict S796/S797 grammar and route
integrity. It cross-checks PCI function identity, vendor/device, BAR index,
base and GOP physical span in separate VM boots. A synthetic fixture never
counts as hardware proof; its role is negative/refusal testing only.

## Evidence classification

- **MEASURED in S795's own separate QEMU boot:** VGA function 00:01.0,
  BAR0 `0x80000000..0x81000000` (exclusive), 16 MiB, mask `0xff000008`.
  Its original command/BAR state was restored. The 4,096,000-byte GOP
  framebuffer is inside this measured range.
- **CORRELATED BUT NOT MEASURED in S796 boots:** the observed VGA BDF,
  vendor/device and BAR0 start match S795. This correlation **does not**
  carry over the S795 measured size into the separate S796 QEMU boots.
- **UNMEASURED in both S796 topologies:** all 8 baseline and 10 bridged BAR
  sizes. The bridge test gives two forwarding *upper bounds*: from
  `0x81000000` up to `0x81200000` (2 MiB) for non-prefetchable memory,
  and from `0xC000000000` to `0xC020000000` (512 MiB) for prefetchable memory.
  They are not measured BAR apertures, accessible MMIO permissions or
  proof that full ranges fit those windows. The corresponding endpoints
  remain blocked until isolated reversible measurements are obtained.
- I/O BARs lack a checked bridge I/O-window contract. PCIe and physical
  devices outside this emulated topology remain unverified.

The output's `size_bytes=None` for every S796 BAR and
`production_cr3_authorized=false` / `mmio_mapping_authorized=false`
are deliberate conservative constraints, not missing code. Refuse any
non-matching exact source SHA, hardware exit, BAR identity, recorded route,
restoration flag or a second observed memory BAR base inside the measured VGA
range. A successful audit **does not** prove that unmeasured BAR ranges do
not overlap other resources; it only refuses known-base conflicts.

This stage writes only a CI report, test code and documentation. It does not
probe live BARs, configure PCI, map MMIO into ordinary Desktop, add a driver,
or modify production boot code. It does not resolve effective PAT/MTRR,
interrupt routing, DMA/IOMMU, UEFI runtime or all physical owners.

**Next:** obtain dedicated QEMU-measured complete aperture lengths of
selected downstream BARs, prove full bridge forwarding and safety before
mapping; separately establish effective caching/IRQ/runtime contracts.
Hardware-readiness of ordinary Desktop and foundation completion remain
**NOT ACCEPTED** until integrated and QEMU-verified with working apps/files.
