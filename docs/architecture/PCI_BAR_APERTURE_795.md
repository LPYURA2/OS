# StyleOS 795 — isolated PCI BAR aperture sizing, not a production device driver

Accepted baseline: 794 `0a1ebcb9d491352a9559689a31126a599ceec1d6`.
Branch: `feature/styleos-pci-bar-aperture-795`.

794 observed the BAR *base* at 0x80000000, but the BAR register does not
encode aperture size. 795 measures the sizing mask of the matching VGA BAR in
a disposable, single-CPU terminal QEMU boot, after ExitBootServices and CLI.

The standard sizing method temporarily writes all ones to a BAR. This is NOT
read-only PCI access: the test saves CF8, PCI command, BAR low/high, disables
I/O and memory decode and bus mastering, probes the mask, restores BAR and
command, checks their readback and restores CF8. PCI command writes are 16-bit,
avoiding PCI status W1C bits. This must NEVER run in normal Desktop or with
another CPU/driver owning the device. Faults/power loss while the register is
temporarily changed cannot guarantee hardware restoration on real devices.
The isolated disposable QEMU environment contains this risk.

The pure decoder validates memory/type attributes, 32/64-bit mask,
power-of-two page-granular size, aligned base, overflow and whole-GOP coverage.
ASAN/UBSAN/COFF, negative UART fixtures and real QEMU debug-exit=33 are all
mandatory and distinct evidence. Synthetic tests alone are not hardware proof.

Only ONE matching display BAR on QEMU bus zero is measured. No PCI hierarchy
or other MMIO inventory, effective PAT/MTRR, IRQ/APIC, EFI runtime or
production CR3 authorization. Normal Desktop source and EFI must not change.
Next: enumerate bridges/remaining BAR resources, cache policy and IRQ/runtime
before attempting a separate production mapping closure test.
