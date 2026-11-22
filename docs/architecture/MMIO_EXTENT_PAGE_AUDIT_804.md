# Stage 804 — complete known MMIO extents, no global alias assertion

## Intended evidence

The terminal QEMU/OVMF EFI reads an inherited firmware page table root after
`ExitBootServices` **without switching CR3 or writing device MMIO/PAT/MTRR**.
Unlike S803 (three address samples), S804 verifies every 4 KiB page covered by
three *specified physical extents* in the **same boot** and emits read-only
physical-leaf segments into UART:

| Resource | Physical interval [start,end) | Length provenance |
| --- | --- | --- |
| GOP pixel surface | `0x80000000–0x803E8000` | Captured GOP `framebuffer_size=4,096,000` |
| VirtIO Serial BAR1 | `0x81000000–0x81001000` | S799 QEMU BAR sizing: 4 KiB |
| VirtIO Serial BAR4 | `0xC000000000–0xC000004000` | S799 QEMU BAR sizing: 16 KiB |

S795 separately measured the **16 MiB VGA BAR aperture**; the 804 GOP test
covers *the pixel surface*, not all unused VGA aperture bytes. BAR sizes for the
VirtIO device are accepted S799 references in a fixed, matching QEMU fixture;
804 does not size PCI BARs again or use the complete PCI/MMIO inventory.

## Enforcement

- For each named range, create a checked, bounded, aligned interval and walk
  every 4096-byte address. Require identity translation, a present PTE leaf,
  the same leaf across each reported segment, valid PAT selection, and MTRR UC.
- Verify the end **byte** of every segment and the count of pages. No gaps,
  overlaps or unchecked tails are permitted.
- Recompute results in a separate Python parser, including MTRR matching for
  all **1005 pages** (1000 GOP + 1 BAR1 + 4 BAR4).
- Require a real QEMU/OVMF exit 33 and preserve raw UART, runner logs, hash,
  negative fixtures, host ASAN/UBSAN, COFF, and the prior hardware regressions.
- Deny production CR3 unconditionally. The ordinary Desktop/boot ABI stay
  unchanged; all tests execute in a separate terminal QEMU image.

## Remaining boundaries

This does **not** enumerate alternative virtual aliases, other MMIO devices,
all VGA aperture bytes, concurrent CPU translations, PCI BAR ownership on
real machines, DMA/IOMMU, IRQ/APIC, or UEFI runtime lifecycle. It establishes
an exhaustive page-level audit *only for the named physical intervals* in the
fixed single-CPU QEMU/OVMF configuration. Other machines and release targets
must independently validate their cache types, ranges, and safe mapping policy.

The next gate S805 should investigate alias coverage or IRQ/APIC with a
bounded, independently testable hardware scope. Ultimately the kernel needs
an accepted production CR3 and full Desktop/process/.tea/file integration.
