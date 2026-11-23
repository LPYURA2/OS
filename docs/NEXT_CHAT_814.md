# StyleOS — continuation point 814

Accepted source basis S813:
`9e93e47a811cf7051279fe08e7670cba39bd32a2`.

S814 task branch:
`feature/styleos-uefi-rootbridge-audit-814`.

Before further changes: compare current exact branch SHA with full
GitHub Actions SUCCESS and independently check recovered source ZIP,
ordinary ISO, UART, QEMU exit codes, JSON, SHA256 and unchanged prior
artifact hashes. Do not accept intermediate commit evidence as final.

S814 adds diagnostic-only UEFI PCI Root Bridge I/O `Configuration()`
resources copied before ExitBootServices; correlates explicit firmware
bus ranges and segment-zero `SegmentNumber` against actual CF8/PCI
bridge enumeration and S813 MCFG. `_CRS`/`_SEG`/`_BBN` ACPI AML was
NOT evaluated. Physical ECAM, IOMMU ownership, DMA domains and own CR3
are NOT validated. Keep DMA disabled.

Next separate stage must resolve actual ACPI hostbridge resource/segment
namespace including ECAM motherboard reservation and vendor IOMMU
ownership / aliases / exceptions before ANY DMA authorization. Then
real hardware DMA deny/allow + fault/invalidation, UEFI runtime, own
production CR3, followed by integrated native kernel+Desktop+
Processes+.tea+StyleFS file acceptance.

Only after the integrated hardware gate can the owner be told the
StyleOS foundation is finished.

Product order afterwards: full Processes; independent native .tea;
users by Welcome/Lock PDF; StyleFS/Explorer by approved PDF with
«Быстрый доступ»; .tea installer, App World, internet/browser/audio/VPN.

No main or accepted-branch changes, no Desktop/palette/mockup changes,
no force push/merge/release. Each stage in its own branch with full
CI-success source ZIP, ISO, hardware logs, hashes, technical report,
continuation and next-chat message.
