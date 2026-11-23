# S813 — read-only ACPI MCFG / ECAM PCI-segment correlation

**Baseline:** accepted S812 `e15d6eb84f26d0ac008122ab758df80d1a42de1b`.
**Task branch:** `feature/styleos-mcfg-segment-audit-813`.
**Status:** candidate; only the exact-head full GitHub Actions SUCCESS with retained
original evidence establishes completion of this diagnostic stage.
**Purpose:** close one missing PCI topology evidence boundary before any IOMMU DMA
permissions are considered. **The StyleOS foundation is NOT complete.**

## New diagnostic-only contract

Use `python3 tools/build.py --arch x86_64 --test-pci-mcfg-813 --output
build/s813` to build an isolated x86-64 UEFI terminal image. The normal EFI,
Desktop, process manager, .tea, StyleFS and application logic are not changed.
The S812 PCI bridge model is linked into this *new* test image; no prior
S812 test or archived report is altered.

1. Before `ExitBootServices`, validate RSDP/XSDT through the existing S806
   reader, select precisely one ACPI `MCFG` by signature, validate the complete
   36-byte ACPI checksum/length, reserved eight bytes, and each 16-byte PCI
   configuration-base allocation structure, and copy the table into a finite
   image-owned static buffer. The table must have 1–32 entries (max 4096 bytes).
2. Validate each allocation's 1MiB ECAM alignment, nonzero base, ascending
   declared bus numbers, zero reserved fields, and bounded base + bus-offset
   arithmetic. Refuse overlapping `(segment, bus)` claims and overlapping
   physical ECAM spans *even when PCI segment numbers differ*. A parser error
   leaves the caller's previous output intact.
3. ECAM offset is computed using the PCI **bus-zero base**, even when the
   allocation's first bus is not zero: `PA = base + (bus << 20) + (dev << 15)
   + (function << 12) + config_offset`. No physical memory dereference is
   made. Computed PAs are metadata only, **not** newly mapped addresses.
4. After EBS, observe segment **0** using the preexisting legacy CF8/CFC
   read-only PCI hierarchy walker, including a real one-bridge and a real
   two-bridge QEMU/OVMF scenario. For each visited bus, require exactly one
   firmware MCFG allocation that covers that bus. Record the base/end of
   bus-zero ECAM as an *ACPI claim*. A missing bus or ambiguous allocation
   fails the diagnostic.
5. Reuse the inherited read-only Intel DMAR and AMD IVRS scope audit to keep
   vendor diagnostic data connected to that same observed PCI topology.
   This is still *not* proof of IOMMU routing, device or controller ownership.
6. A missing, duplicated, corrupt or mismatched MCFG fails closed. The no-IOMMU
   machine still checks MCFG coverage, while granting no DMA permissions.

### Boundaries NOT proven by S813

* There is **no ECAM MMIO read or write**, no new page table mapping, no proof
  of cache UC/PAT/MTRR or physical MMIO ownership, no `_CRS` host-bridge
  resource validation, no ACPI namespace walk, no hot-plug verification, and no
  independently enumerated PCI segment other than **segment 0**. Nonzero
  segment MCFG entries are parsed in the **host fixture** but no real nonzero
  segment device is observed.
* The advertised ACPI MCFG bus window may not match current host bridge `_CRS`
  (or platform-specific quirks). See Linux PCI host bridge documentation:
  https://docs.kernel.org/PCI/acpi-info.html. S813 is **not** a safe general
  PCI configuration-space driver.
* DMA aliases, RMRR/IVMD exclusions, multi-unit mappings and device quirk
  exceptions still need complete and physically verified IOMMU ownership and
  policy. Device-level bus-mastering cannot be assumed to be isolated even if
  the MCFG coverage count equals observed buses.
* IOMMU root/device/context tables, DMA domains, fault counters, flushes and
  invalidations, actual forbidden DMA **deny** tests, DMA **allow** after
  domain grant, and no-IOMMU restrictions are NOT implemented/validated here.
* The normal Desktop CR3 remains unchanged. UEFI runtime, general physical
  memory ownership and integrated native kernel/Desktop/.tea/StyleFS acceptance
  are not included.

**Security invariant:** `pci813_allow_dma()` returns zero unconditionally;
no ACPI/PCI match, ECAM address, alias or bridge transit yields a grant.

## Sources and tests

| File | Responsibility |
|---|---|
| `include/styleos/pci_mcfg_813.h` | bounded MCFG allocation, correlation and explicit DMA-deny API |
| `kernel/pci_mcfg_813.c` | checksum, table structure, physical overflow / overlap, segment and bus queries |
| `boot/x86_64/pci_mcfg_813.c` | pre-EBS ACPI image copy, post-EBS segment-zero PCI walk and terminal UART report |
| `tests/test_pci_mcfg_813.c` | host synthetic MCFG with two segments and nonzero start bus, invalid entries, denied DMA, two-bridge topology |
| `tools/pci_mcfg_813_verify.py` | strong exact grammar, x86 QEMU debug-exit 33, limits, required deny policy |
| `tools/test_pci_mcfg_813_negative.py` | forged/unsafe UART evidence refusal cases |
| `tools/test_pci_mcfg_813.py` | real QEMU+OVMF no-IOMMU, Intel, single and double bridge Intel, AMD (if supported) |
| `tools/build.py`, `boot/x86_64/uefi.c` | isolated new test flag and terminal hooks only |
| `.github/workflows/styleos-709-to-730-recovery.yml` | ASAN+UBSAN, COFF, refusal, full real QEMU test and retained logs |

Locally runnable sanitized model check:

```sh
clang -std=c11 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -I include \
  kernel/pci_mcfg_813.c kernel/pci_requester_812.c \
  kernel/dmar_808.c kernel/iommu_platform_810.c \
  tests/test_pci_mcfg_813.c -o build/s813-host
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/s813-host
python3 tools/test_pci_mcfg_813_negative.py
python3 tools/test_pci_mcfg_813.py --output build/s813-qemu
```

For completion, require the exact-head full workflow SUCCESS, original UART
from each mandatory QEMU configuration with exit 33, mode-specific PCI bus
and MCFG records and SHA256, inherited S812 full regression, ordinary ISO
binary sameness with S812, full exact source ZIP, checksum file and evidence
archive. The Linux/CI VM is distinct from Termux, VirtualBox and real PC.

## Next safe steps

1. Complete verified ACPI PCI host bridge `_CRS` + `_SEG` and reserved UC ECAM
   mapping with an **isolated** test, or explicitly document uncovered roots.
2. Fully audit AMD IVHD aliases, IVMD exceptions, Intel DMAR RMRR/include-all,
   IOMMU PCI units and actual source IDs before *any* DMA allow.
3. Only in a separate experimental branch, build a real QEMU IOMMU domain
   lifecycle with owned buffers and safe root/device table grants. Test true
   device-generated DMA **deny**, then **allow**, faults and invalidations
   with SHA-bound hardware UART, no-IOMMU fallback and rollback.
4. Finish UEFI runtime and own production CR3; then integrated real x86
   kernel + Desktop + Processes + independent .tea + StyleFS file acceptance.
   Announce foundation complete only after that independent acceptance.
5. Then keep the approved product sequence: full Processes; independent .tea;
   users/login/lock by `StyleOS-Welcome-and-Lock-Screens.pdf`; StyleFS and
   Explorer with “Быстрый доступ” by `StyleOS-Explorer-Mockups.pdf`; installer,
   App World, internet, browser, audio and VPN.
