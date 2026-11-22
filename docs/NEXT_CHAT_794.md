# StyleOS — handoff after stage 794

Base accepted 793: `84284906f79a5339b275278ffea1ebd49d5d771a`.
794 branch `feature/styleos-pci-gop-bar-observation-794`.

**Do not accept stage 794 until exact final SHA, successful mandatory full
GitHub Actions, original QEMU UART with PCI BAR match and debug-exit 33, and
CI source/ISO artifacts are verified.** The files in this repository are an
engineering record, not acceptance by themselves.

This stage adds only terminal x86 Q35 bus-zero PCI configuration scanning to
correlate the **start address** of one display memory BAR to GOP physical
framebuffer start. No BAR sizing, full PCI enumeration, effective cache
verification, IRQ routing, or UEFI runtime policy. No production CR3
activation, Desktop changes, main merge or release. All unsupported claims
remain explicitly false.

Next stage: close PCI BAR aperture size, bridges/MMIO, ACPI/IRQ routing,
PAT/MTRR plus EFI runtime before even a private production mapping proof.

Long-term approved order: finish foundation -> real Processes -> independent
native `.tea` alongside Desktop -> users/login/lock strictly per approved
`StyleOS-Welcome-and-Lock-Screens.pdf` -> StyleFS and Explorer strictly per
`StyleOS-Explorer-Mockups.pdf` (`Быстрый доступ`) -> .tea installer/App World
then network/browser/audio/VPN. Keep Desktop UI and palettes unchanged.

Each stage deliver: full source ZIP from successful CI, CI ISO, evidence ZIP,
exact SHA / Actions, final report, handoff and separate ready-to-send message
for a new chat. Explicitly tell owner when foundation is truly completed;
do not call it done based only on isolated EFI hardware tests.
