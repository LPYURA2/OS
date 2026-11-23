# StyleOS S821 — real ACPICA core COFF admission and guest checksum probe

**Task branch** `feature/styleos-acpica-core-link-821` from accepted S820
`b4d768a522e5f5e8069c9da9f2f3d560cb16188d`.

**Narrow objective:** compile the *complete* production ACPICA interpreter
component set (dispatcher, events, executer, hardware, namespace, parser,
resources, tables, utilities) from **pinned official 20260930 sources** into
freestanding x86-64 COFF objects, then invoke a real upstream ACPICA table
checksum function against an original firmware-provided DSDT **inside a
separate StyleOS diagnostic guest** after ExitBootServices.

This is a necessary native code/linking admission gate, **not** an assertion
that full ACPICA/AML runs in the guest. The archive is a static library: only
symbol-reachable objects are necessarily linked into the test EFI image. The
QEMU guest executes the upstream `AcpiUtChecksum` function from its actual
COFF source, **not** `AcpiEvaluateObject`, not `AcpiLoadTables` and not a
complete OSL. The source symbol inventory cannot be promoted to runtime
interpreter evidence. The ordinary Desktop ISO is unchanged.

## Provenance and licensing

- Official source archive:
  `https://github.com/open-acpica/acpica/releases/download/20260930/acpica-unix-20260930.tar.gz`
- Exact SHA-256:
  `aa18901b92e30749be0edc3081c8d550c61fce4fa37546fc6a65d367a4ae71a5`
- Source validator from S820 rejects duplicates, traversal, links, oversize,
  absent headers and wrong SHA; 821 compiles **only** this verified tree.
- Official upstream release is available under **BSD-3-Clause OR GPL-2.0-only**.
  StyleOS intends the BSD-3-Clause option; retain all upstream copyrights,
  full BSD license text and disclaimer in every binary distribution that
  contains ACPICA code, and track updates. This does not change the StyleOS
  project's own licensing automatically. See official release and
  `https://github.com/open-acpica/acpica/blob/20260930/LICENSE.BSD-3-Clause`.
- No ACPICA source is blindly vendored into repository. The tested binary may
  be rebuilt deterministically from the verified release archive in CI.

## Scope and safety

- The test-specific `--test-acpica-core-821` mode inherits S819's bounded
  ACPI table snapshots and denies all hardware OperationRegion operations.
- Before EBS, the existing capture copies the firmware DSDT. After EBS,
  genuine ACPICA `AcpiUtChecksum` verifies its contents and rejects one
  deliberately corrupted **image-local** control buffer. No firmware writes.
- Later, the inherited S819 terminal test checks private table mapping,
  all six denied hardware calls, the PCI bridge topology, unchanged DSDT,
  MCFG and IOMMU observation. All DMA permissions remain **denied**.
- A distinct strict UART verifier checks exactly three S821 records, their
  order before inherited S818/S819 evidence, actual DSDT length and the
  explicit `aml-executed=no` policy. Negative mutation tests reject forged
  PASS, hardware grants, role changes, wrong lengths and bad exit codes.
- Five real QEMU/OVMF profiles: no IOMMU, Intel, Intel with one bridge,
  Intel with two bridges, AMD if virtual AMD-IOMMU is available. Required
  debug-exit 33; original UART and SHA-256 retained. A QEMU run is not a
  physical PC/VirtualBox/Termux hardware result.
- Full accepted S820/819/earlier regressions and ordinary ISO hash equality
  remain mandatory in the same exact-HEAD successful GitHub workflow.

## Not proven; next steps

**Guest AML execution is NOT yet proven.** Complete native OSL allocator,
serialization, event/interrupt policy, PCI/ECAM/OperationRegion handlers,
AcpiLoadTables / namespace evaluation, SSDT on a real firmware, actual
`_CRS`/`_SEG`/`_BBN`, ECAM resource reservation, IOMMU requester ownership,
hardware DMA deny/allow/fault/invalidation, UEFI runtime, production CR3,
and full Desktop+kernel+Processes+native .tea+StyleFS integration remain
outstanding.

Next distinct stage must implement a bounded audited ACPICA OSL and use the
upstream `AcpiEvaluateObject` **inside** a disposable diagnostic guest, with
real fault isolation and deny-all hardware access. Do not enable the AML engine
on an untrusted physical PC without controlling OperationRegion requests.

**The StyleOS foundation is NOT COMPLETE.**

## Diagnostic OSL logging boundary (821)

The static ACPICA checksum object has dependencies on `AcpiOsPrintf` and
`AcpiOsVprintf`. The isolated guest provides bounded serial-only wrappers:
format arguments are deliberately not expanded, and any unexpected diagnostic
is visible in UART. These wrappers are **not** a production printf/OSL and
must not be used to claim general AML support. The selected 162 upstream
release-mode components compile as COFF; guest linkage and runtime evidence
still require exact-HEAD QEMU/OVMF and complete CI SUCCESS.
