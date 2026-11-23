# StyleOS — continuation from stage 819

S818 accepted baseline: `1fa46a89239c5faabd1a91b6ca85ceef80866b4d`.
S819 branch: `feature/styleos-acpica-guest-table-gate-819`.
Before accepting, verify exact HEAD + GitHub Actions SUCCESS, original CI
ZIP/ISO, all five QEMU UART and DSDT with actual SHA, parent regression,
ASAN/UBSAN/COFF and strict negative tests. Do not replace hardware evidence
with model tests or AcpiExec host replay.

S819 demonstrates freestanding ACPICA-style callbacks and bounded mapping of
original-address ACPI DSDT/SSDT *private copies* in QEMU. This is NOT a complete
ACPICA OSL and no real ACPICA engine is linked in the guest. Memory, port and
PCI hardware callbacks all deny. All DMA remains denied.

**Next critical milestone**: vendor/pin and build *real* ACPICA interpreter
source in a diagnostic image, implement audited freestanding OS Services Layer
with bounded memory/locks/callbacks, restrict OperationRegion, and prove
actual namespace method evaluation plus SSDT and ACPI resource results.
Then prove IOMMU ownership, real DMA deny/allow/fault/invalidation, UEFI
runtime, own production CR3 and complete integrated ordinary Desktop/kernel/
Processes/.tea/StyleFS hardware acceptance. Never claim the foundation done
before that integrated gate.

Then full Processes → independent .tea → users/Welcome/Lock → StyleFS/Explorer
with “Быстрый доступ” → .tea installer → App World → networking/browser/
audio/VPN. New branch per stage; keep main, accepted refs, Desktop and
approved mockups untouched; no releases or force-push.
