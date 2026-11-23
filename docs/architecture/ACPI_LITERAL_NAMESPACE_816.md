# StyleOS 816 — bounded AML literal-only namespace experiment

Source base: accepted 815 `b6a84c48c58434dea15aac42f4fa68ed7017451d`.
Task branch: `feature/styleos-acpi-literal-namespace-816`.

## Reason

815 confirmed checksummed ACPI FADT → DSDT and emitted actual QEMU DSDTs.
It **did not** interpret ACPI AML or SSDT definitions. Device ownership,
`_CRS`, `_SEG`, `_BBN`, PCI ECAM reservation, IOMMU remapping and DMA
translation remain unproved. This stage is a deliberately **narrow** parsing
experiment, *not* an ACPICA-complete interpreter or a production ACPI service.

## Test-only execution path

The new **separate** x86-64 `--test-acpi-namespace-816` image retains 815's
PCI/MCFG/RootBridge diagnostics. It validates and copies checksummed SSDTs
from the XSDT into image-owned BSS before ExitBootServices (max eight SSDTs,
24 KiB each); any over-capacity, invalid length or checksum rejects capture.
After EBS no firmware protocol is invoked. In particular, **no OperationRegion
handler is installed or invoked**, no ECAM MMIO is dereferenced, no PCI/IOMMU
writes, interrupt routing or DMA are enabled. The parser consumes the exact
captured DSDT plus SSDTs in XSDT order. The guest prints a bounded namespace
summary and complete checksummed DSDT/SSDT byte streams to UART (32-byte
frames), for independent reassembly and SHA-256 binding to real QEMU runs.

The pure C reader (`kernel/acpi_namespace_816.c`):

- validates each ACPI header/length/checksum before parsing;
- parses only structural `Scope`, `Device`, `Name` and zero-argument `Method`
  objects whose bodies are exactly `Return(literal)`;
- supports bounded AML package lengths, name prefixes and direct integer or
  fixed byte-buffer literals, with hierarchy depth <=8, 128 objects and
  256-byte literal buffers;
- merges DSDT/SSDT in deterministic sequence, rejects duplicate fully
  qualified names, unknown/malformed package lengths and unsupported AML;
- treats OperationRegion as **strictly non-executable**; if unsupported AML is
  encountered, it **stops scanning the remainder of that term list** instead
  of falsely finding a byte sequence inside embedded buffers or method code;
- only supplies synthetic-fixture literal `_SEG`, `_BBN` and `_CRS` evidence
  when all input term lists are from this exact limited grammar. A literal
  `_CRS` resource template is not validated host-bridge ownership or access.

Actual QEMU AML can use methods, OperationRegions, object references and AML
opcodes beyond the accepted subset. Those paths result in
`complete=0` / `literal-ready=0`, **not** a faked hostbridge proof. Even
`complete=1` for the subset never means arbitrary full AML namespace
interpretation. The output policy always reports namespace-complete=no,
ACPI `_CRS/_SEG/_BBN` hardware evidence=no, DMA=no, production CR3=deny.

Note: the pure test module uses fixed static scratch for deterministic bounded
memory without a kernel heap; it is not re-entrant or SMP capable. A real ACPI
runtime needs an interpreter such as an audited port of ACPICA or equivalent,
including controlled OperationRegion and verified device/namespace lifecycle.

## Tests and acceptance

1. C host fixtures under ASAN/UBSAN: DSDT plus SSDT merge; Device/Scope;
   literal `_SEG`, `_BBN` and method-return Buffer `_CRS`; duplicate names;
   unknown bytecode; corrupt checksum; denied OperationRegion and DMA.
2. Freestanding COFF builds for model and UEFI adapter.
3. Strict negative UART parser refusing forged namespace completion,
   performed `_CRS`, unverified DMA, truncated/duplicated SSDT streams.
4. Real QEMU/OVMF: disabled IOMMU, Intel VT-d, Intel with one bridge, Intel
   with two bridges, AMD-Vi (if available). Each must terminate `debug-exit 33`
   and expose original checksum-valid DSDT/SSDT UART evidence; no unsupported
   namespace claim may become a DMA grant. The SSDT *model* is exercised with
   an explicit fabricated test fixture; stock QEMU may expose zero SSDTs.
5. Full inherited workflow and reproducible **unchanged ordinary ISO** versus
   accepted 815, successful CI archive, complete raw UART/JSON, exact SHA.

## Next foundational gate

Do not start DMA domains from fake ACPI ownership. Identify a deployable,
security-bounded complete AML engine, interpret real `_CRS/_SEG/_BBN` and
motherboard ECAM resources, correlate them with MCFG/UEFI RootBridge and
IOMMU requesters including aliases. Only then attempt isolated DMA deny/allow
on disposable devices, verifying fault handling/invalidation. Complete UEFI
runtime and own production CR3, followed by full ordinary kernel + Desktop +
Processes + `.tea` + StyleFS file operations **integration hardware** tests.

**Foundation NOT COMPLETE.** `main`, accepted branches, UI, palette, mockups,
ordinary Desktop, applications, file formats and Portable-os are untouched.

References: [ACPI 6.6 AML grammar](https://uefi.org/specs/ACPI/6.6/20_AML_Specification.html),
[ACPI 6.6 Device Configuration](https://uefi.org/specs/ACPI/6.6/06_Device_Configuration.html).
