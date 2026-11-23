# S811: conservative PCI requester scope evidence (diagnostic only)

S810 established ACPI DMAR/IVRS discovery in Intel/AMD QEMU. S811 adds a **bounded parser** of explicit Device Scope BDF entries, and compares the explicitly listed BDF values with a read-only snapshot of **PCI bus 0** configuration using CF8 selection and CFC reads. CF8 select is a port write, but it does not change any device configuration registers.

- Intel DRHD: only PCI endpoint (scope type 1), **one** PCI path pair and valid `start_bus` are decoded as explicit BDF entries; bridge scopes, multihop paths and other scope types remain unresolved. Include-all is recorded but **does not** imply an exact ownership proof without complete hierarchical enumeration and exclusions.
- AMD IVHD types 0x10/0x11/0x40: limited standard 4-byte `SELECT` (0x02), bounded `RANGE START/END` (0x03/0x04), and `ALL` (0x01) entries. Unknown and extended (8/16-byte) entries are marked unresolved rather than guessed. Range expansion is limited to 256 requester IDs; larger spans are left unresolved. The complete IVHD device entry format is *not* supported yet.
- Cross-controller duplicate Requester IDs cause refusal. An exact entry is an **explicit firmware claim**, not evidence that a device is really attached to this IOMMU on the physical fabric. Only bus 0 is scanned; no downstream PCI bridges are resolved. Unlisted requesters cannot be authorized.
- All parsed snapshots have ACPI checksum and basic format validation from S808/S810. No IOMMU register writes, no page/domain map, no DMA permission, no PCI BAR writes, no production CR3 switching.
- QEMU with no IOMMU, Intel VT-d and AMD-Vi are checked separately. AMD is skipped only if the installed QEMU lacks `amd-iommu`, in which case its hardware support is **not verified**, while the AMD parser is still required under ASAN/UBSAN.

## Remaining blockers

1. Full PCI enumeration across bridges and segments, with verified parent/secondary/subordinate buses, including Intel bridge and multi-hop DRHD paths.
2. Complete AMD IVHD device entry parsing, aliasing and exclusions, a sound mapping from PCI requester IDs to IOMMU units, and handling multiple controllers.
3. Sound DMA domain lifecycle and permissions, fault recording, invalidation ordering, experiments denying and allowing real DMA, and a secure no-IOMMU fallback policy.
4. UEFI runtime and production CR3 plus normal Desktop/files/processes/.tea integration.

**Foundation not complete; S811 is a read-only hardware discovery milestone, not DMA isolation.**
