# StyleOS S817 — continuation point

Accepted base: S816 99e8d873aba5725e77760d0360280763c0d14fb9,
Actions https://github.com/LPYURA2/OS/actions/runs/45219189553 (SUCCESS).

Task branch: feature/styleos-acpica-replay-817. Check exact final SHA
and successful CI before acceptance. Verify source archive, ISO, original
QEMU UART, real DSDT/SSDT, ACPICA binary hash, synthetic AML, and logs.

S817 introduces ACPICA AcpiExec on a **host-only simulated replay** of
firmware AML. This is not ACPICA embedded into StyleOS, production-safe
OperationRegion handling, real host bridge _CRS/_SEG/_BBN evaluation,
ECAM reservation, IOMMU ownership, DMA isolation, UEFI runtime or own CR3.
No permissions have been opened.

Next stage: establish and test ACPICA OS services/runtime within freestanding
StyleOS with deny-by-default hardware region policy, then verified firmware
root bridge/MCFG, actual IOMMU ownership and DMA hardware deny/allow,
fault handling/invalidation, runtime UEFI, production CR3 and combined
kernel+Desktop+Processes+.tea+StyleFS hardware acceptance.

Then per owner: Processes full resource/PID/tabs; independent .tea;
users/Welcome/Lock; StyleFS/Explorer with «Быстрый доступ»;
installer, App World, internet/browser, sound and VPN.

Preserve main, accepted branches, Desktop/palette and mockups, history and
Portable-os. Each stage = new branch, successful CI ZIP, ISO, hardware
evidence/SHA, report, continuation, next-chat message.

**Foundation NOT COMPLETE.**
