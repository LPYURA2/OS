# StyleOS 794 — physical PCI BAR/GOP base provenance (Q35, terminal only)

## Objective and scope

Stage 793 observed a GOP framebuffer at physical `0x80000000`, 4,096,000 B,
but no UEFI memory-map descriptor covered it. The **new** 794 terminal EFI
`--test-pci-probe-794` reads the PCI configuration mechanism #1 via CF8/CFC
on **bus zero only** (all device numbers 0–31 and functions 0–7). It checks
that an actual video-class PCI memory BAR decodes to exactly the GOP physical
start. This links the address to a PCI device BAR configuration, not a
UEFI memory descriptor. It does not infer the aperture's size or cache policy.

It runs after ExitBootServices, on a single CPU with IF disabled and the
inherited CR3, and writes only CF8's configuration *address selector*, restoring
its original value. It **never** writes PCI configuration data at CFC, probes
BAR size with all-ones writes, touches framebuffer memory or changes IRQ/PAT/
MTRR/CR3. Q35 and `-vga std` are explicit environment requirements. The
probe is terminal: it ends QEMU with ISA debug-exit 33, not UEFI return.

## Engineering and verification

- `kernel/pci_probe_794.c` parses type-0 BARs, including 64-bit BAR pairing,
  I/O distinction, raw register evidence and refusal of reserved encodings.
- `boot/x86_64/pci_probe_794.c` scans bus zero PCI config registers and
  demands **exactly one** display-class memory BAR matching the GOP base.
  It emits real BDF, vendor/device, BAR raw bits and decoded base to UART.
- `tests/test_pci_probe_794.c` and `tools/test_pci_probe_794_host.py` validate
  C parsing with ASAN/UBSAN plus freestanding COFF compilation.
- `tools/test_pci_probe_794_negative.py` rejects counterfeit, malformed,
  contradictory UART fixtures. These are **not** hardware tests.
- `tools/test_pci_probe_794.py` runs actual QEMU/OVMF and requires debug-exit
  33 and the original UART evidence. CI fails if QEMU/firmware is missing.
- Full existing Desktop/host/hardware regressions remain required in CI,
  and the production EFI/ISO must remain identical to accepted 793.

## Unresolved dependencies

The match proves only *BAR start address* equality for one bus-zero device.
The BAR's aperture size is **unknown** because we never perform the hardware
size-probe writes. Bridge/subordinate PCI buses are not traversed; it is not
a full PCI device/MMIO inventory. PCI BAR assignment itself is not a proof of
memory ownership, safe CPU aliases or effective cache type. Raw PAT/MTRR from
793 are still insufficient; full fixed/variable MTRR and PAT index + alias
resolution is pending. APIC/IOAPIC, IRQ routing, port ownership, ACPI and UEFI
runtime services lifecycle have not been audited.

Therefore the stage reports `bar_aperture_size_verified=false`,
`all_pci_bars_enumerated=false`, `effective_cache_type_verified=false`,
`irq_routing_verified=false`, and **`production_cr3_authorized=false`**.
Success never authorizes a production `CR3` switch. The accepted Desktop,
boot_info v1 and `main` remain unchanged.

Next: separate PCI BAR aperture sizing with guarded temporary decode policy
or firmware/ACPI corroboration, enumerate bridge devices and MMIO, establish
cache attributes and IRQ routing, then test a fully closed production kernel
address-space *in isolation* before touching normal Desktop.
