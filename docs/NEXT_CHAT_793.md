# StyleOS — handoff after stage 793

Base accepted stage 792: `245a28c7511f90ea6e37dbfe199e8ce8b8245015`.
Stage 793 branch `feature/styleos-uefi-hardware-observation-793`.
**Before acceptance** verify the exact commit, complete GitHub Actions
`verify-and-archive` success, real QEMU observation `results.json` with
`qemu_exit_code=33`, original UART and source/ISO artifact from that run.

First QEMU run 45042996026 FAILED and timed out because the GOP BAR
(`0x80000000`, `0x003e8000` bytes) did not overlap any EFI memory-map
descriptor. Corrective revision records this as a **GAP**, not fake MMIO
coverage. Only a separate successful rerun can accept the stage.

793 implements a terminal-only UEFI GOP/descriptor/CPU observation:
actual GOP aperture and intersecting EFI descriptors, raw IA32_PAT and
conditional MTRR_DEF_TYPE, current CR0/CR3/RFLAGS, legacy PIC masks and
I/O-port source list. This does not prove effective caching, PCI BAR coverage,
IRQ routing, UEFI runtime call policy or production CR3 closure. No boot_info
v1 ABI change, no Desktop paging switch, no modification of main or accepted
branches. In CI the optional -vga std guest is mandatory for this stage.

Next stage 794: inventory actual PCI BAR/MMIO, IRQ routing and runtime
services lifecycle, then design a separately attested production mapping
closure. Maintain deny-by-default and keep Desktop image unchanged.

Product plan remains: finish foundation → full real Processes → independent
native .tea alongside Desktop → users/login/lock strictly per approved PDFs →
StyleFS and Explorer strictly per approved PDF (`Быстрый доступ`) → installer,
App World, network/browser/audio/VPN. Every stage needs source ZIP, ISO,
evidence ZIP, final SHA/CI, technical report, handoff, separate new-chat text.
