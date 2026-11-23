# StyleOS 812 — PCI bridge hierarchy and AMD IVHD requester audit

Status: experimental task branch, **not accepted until exact-head CI and archived
evidence have been checked**. Based directly on accepted S811
`16f03f5f182592f70b3271358cfcd9e8ab2e46c2`.
Branch: `feature/styleos-pci-bridge-ivhd-812`.

## Purpose and trust boundary

Expand the S811 read-only requester-scope audit without accidentally converting
ACPI inventory into IOMMU authorization. All new implementation is included
**only** by `--test-pci-requester-812` and terminates after its diagnostic
snapshot. Ordinary boot, Desktop, files, processes, native .tea and GUI are not
modified by this test. No production CR3 switching, no IOMMU register writes,
no DMA domains, no DMA grants, no fault/invalidations, no UEFI runtime changes.
The unconditional `pci812_allow_dma()` returns zero.

### PCI topology

The diagnostic traverses bus 0 and each accepted secondary bus through PCIe
bridges, using only legacy CF8 selection and CFC read-only configuration reads.
A bridge must have header type 1 and class 06:04, matching primary bus, a
nonzero secondary bus, subordinate >= secondary, and a secondary bus not
previously visited. Each child subordinate must not exceed its ancestor bound.
All function IDs are bounded (4096 maximum) and bus traversal is bounded
(256). A 16-hop limit applies to DMAR device scope resolution. The saved CF8
value is restored on successful exit; invalid topology terminates with failure.

The topology is **not** an exhaustive ECAM/MCFG discovery mechanism.
Unsupported headers, PCI segments other than segment 0, devices unreachable
from legacy CF8, and multiple root buses are not silently accepted.

### Intel DMAR

DMAR data is validated via the existing S808 parser, copied before EBS, and
decoded only after a complete bounded PCI walk. For a type-1 endpoint scope,
each hop must resolve to a real bridge until the final endpoint. The final
Requester ID is an explicit firmware endpoint entry, not proof that its actual
DMA transactions pass through a particular remapping unit.

Type-2 PCI subhierarchy bridge scopes are recorded separately: a known bridge
does **not** imply resolved downstream IOMMU ownership. Include-all DRHD is
counted, not applied as a permission. DMAR RMRR is recorded as a reserved-memory
exception requiring separate DMA handling; it is not a permission. Unknown
scopes or inaccessible routes stay unresolved.

### AMD IVHD/IVMD

The S812 audit adds bounded handling of the following known IVHD device
entry types: SELECT (0x02), start/end range (0x03/0x04), alias select
(0x42), alias range start (0x43), extended select (0x46), extended range
start (0x47), and special (0x48) as **unresolved**. ALL (0x01) is counted,
not expanded. Variable/unsupported entries are never guessed into grants.
Alias records are intentionally separate from exact requester ownership.
An alias source is not a directly owned requester; conflicting exact/alias
or cross-unit claims fail the audit. Expansion of any range is capped at
256 IDs. Unknown/extended semantics not safely parsed remain unresolved.

IVMD types 0x20..0x22 are counted as mandatory policy exceptions, **not**
translated into mappings. Only PCI segment zero is handled by the current
topology, and a nonzero segment stays unresolved. Intel and AMD ACPI tables
are mutually exclusive according to the S810 selector.

### Trust guarantees and remaining security gaps

- A verified match is a firmware assertion correlated with a read-only PCI
  snapshot; this is **not** complete IOMMU ownership.
- Alias, RMRR, IVMD, INCLUDE_ALL, unsupported special records, nonzero segments,
  bridge subhierarchies and unknown records **never** authorize DMA.
- No-IOMMU mode is diagnostic only; it must not permit untrusted bus-mastering.
- The accepted QEMU bridge setup proves the traverser can observe at least one
  downstream PCI bus, but QEMU firmware may not publish a multihop DMAR
  endpoint scope. Synthetic host fixtures test explicit three-hop paths,
  *not* same-boot hardware ownership.
- A complete AMD specification implementation, hardware alias tests, multiple
  controllers/segments, ECAM/MCFG, IOMMU root/context/device tables,
  invalidation, fault handling and real forbidden DMA tests remain outstanding.

## Source boundaries

- `include/styleos/pci_requester_812.h`: bounded, diagnostic-only public API.
- `kernel/pci_requester_812.c`: pure PCI graph/DMAR/IVRS/IVHD parsing.
- `boot/x86_64/pci_requester_812.c`: isolated ACPI capture, CF8/CFC probe
  and terminal UART record.
- `tests/test_pci_requester_812.c`: checksums, two-bridge three-hop fixture,
  malformed routes, vendor alias, extended records, RMRR/IVMD and denial.
- `tools/pci_requester_812_verify.py`: grammar, hardware exit code 33 and
  upper-bound checks on UART data.
- `tools/test_pci_requester_812_negative.py`: forged evidence rejection.
- `tools/test_pci_requester_812.py`: QEMU/OVMF disabled, intel, bridge-intel,
  AMD when supported; hashes and raw output retained.
- `tools/build.py`, `boot/x86_64/uefi.c`, main workflow: S812-specific
  build mode, no change to default mode.

## Repeatable testing and delivery

```sh
clang -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I include \
  kernel/pci_requester_812.c kernel/dmar_808.c kernel/iommu_platform_810.c \
  tests/test_pci_requester_812.c -o /tmp/styleos-812-host
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/styleos-812-host
python3 tools/test_pci_requester_812_negative.py
python3 tools/test_pci_requester_812.py --output build/pci-812-qemu
```

The workflow additionally requires x86 COFF compilation, the complete old
regression suite, the unchanged ordinary ISO, reproducible source archive,
QEMU serial logs, JSON, SHA256, and the exact CI head. Do **not** mark this
stage accepted if the full workflow fails or if results are only host fixtures.

Original acceptance base: S811 SUCCESS
https://github.com/LPYURA2/OS/actions/runs/45164125712

S812 CI and artifacts: inspect the latest exact-head run on the task branch;
do not substitute a prior run. Exact artifact SHA-256 must come from the
uploaded CI checksum records, not an estimate.

## Next sequence

1. Confirm the S812 head and entire GitHub Actions job including old regressions.
2. Independently read preserved disabled/Intel/bridge-Intel/AMD UART and
   results JSON, original checksums and the full source ZIP.
3. Extend PCI segment/ACPI requester topology coverage where necessary.
4. **Only then**, separate experimental DMA-domain lifecycle with real deny /
   allow on a disposable QEMU device; fail closed and show actual IOMMU faults
   and ordered invalidation.
5. UEFI runtime, owned production CR3, and full kernel+Desktop+process+.tea+
   StyleFS/file integration after hardware closure.
6. Explicitly tell the owner the foundation is complete **only** after the
   separate full integrated hardware acceptance.

**Foundation not complete.**
