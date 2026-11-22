# StyleOS 799 — terminal downstream PCI BAR aperture proof

**Base:** accepted 798 `1b3bfec10494660fc27615659af5c5d4cdaf8e60`.  
**Branch:** `feature/styleos-downstream-bar-measurement-799`.

A dedicated `--test-pci-range-799` EFI is built separately from Desktop.
Under single-vCPU QEMU/OVMF Q35, after ExitBootServices and CLI, it
selects the PCIe root port `00:1c.0` with secondary bus 1 and the previously
inventoried unused VirtIO Serial endpoint `01:00.0` (`1af4:1043`).

The guest snapshots PCI COMMAND and exactly two endpoint memory BARs:
32-bit nonprefetchable BAR1 and 64-bit prefetchable BAR4. It disables
PCI I/O+memory+busmaster decode by a 16-bit COMMAND write, reads it back,
probes each BAR with all-one sizing mask, and restores the original BAR
and COMMAND before any policy decision and before emitting PASS. It does not
write PCI STATUS (W1C), bridge configuration, MMIO or normal Desktop state.

The memory sizing parser requires an aligned power-of-two size, PCI BAR
flag preservation, no 52-bit physical overflow, a nonzero extent, and
**whole** `[base,end)` containment in the matching parent bridge window.
A true negative result or failed restoration is fatal. The UART verifier
independently checks mask arithmetic, both selected BARs, bridge route,
UART ordering, PCI command restoration and QEMU debug-exit 33. Host
ASAN/UBSAN tests and forged-UART refusals are mandatory too.

### Limits and explicit denials

- Endpoint quiescence is specific to this terminal QEMU boot, not a general
  physical-device or hot-plug procedure. In a real OS the driver must detach,
  DMA be halted and races excluded *before* any sizing probe.
- Only selected BAR1/BAR4 of one QEMU endpoint are measured. Other BARs and
  devices remain unmeasured. Memory bridge windows are routing boundaries,
  **not** independent ownership/caching proofs.
- No reliable effective PAT/MTRR cache policy, IRQ/APIC routing, DMA/IOMMU,
  UEFI runtime contract or general MMIO mapping is established here.
- `production_cr3_authorized=false`. Do not enable a new CR3 in Desktop.
- Stage is accepted only with the **exact** final SHA, completed successful
  GitHub Actions, original QEMU UART, sources/ISO hashes and regression proof.

Next: obtain effective caching and IRQ contracts, then integrate proved
memory ownership/page permissions into ordinary Desktop and test apps/files
in an actual QEMU boot. Foundation completion requires that integration,
not just successful terminal diagnostics.
