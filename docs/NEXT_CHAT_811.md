# StyleOS — stage 811 handoff

Accepted base: S810 `fff59c107172f2438ad34f472c707cd235ddd393`. Candidate branch `feature/styleos-requester-scope-811`.

S811 adds bounded Intel DMAR PCI endpoint scope matching and AMD IVHD SELECT/short RANGE decoding plus real PCI bus0 read-only enumeration in QEMU. Unknown, bridges, extended entries, include-all rules and range spans >256 remain unresolved. All DMA authorization functions fail closed. No production CR3, no DMA domain, no device IOMMU remapping, no user-facing Desktop changes.

Next stage: complete PCI bridge topology and AMD IVHD semantics before claiming general requester ownership; then safely test DMA domain assignment, faults, invalidations and real forbidden DMA accesses. Then UEFI runtime and normal kernel integration.

Foundation NOT complete. After foundation: Processes → independent .tea → users/login/lock → StyleFS/Explorer → App World and network/browser/audio/VPN. Preserve approved UI and all accepted branches.
