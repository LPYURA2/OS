# StyleOS 822 — Native ACPICA AML evaluation (synthetic namespace only)

Parent: accepted S821, 472c9c0e622084ac3ad584f6256d1008c8df1597.
Branch: feature/styleos-acpica-namespace-init-822.

The isolated --test-acpica-aml-822 links official ACPICA 20260930
from the SHA-verified upstream source package, under BSD-3-Clause
(full license retained in CI).

## Narrow boundary
The guest constructs private, checksummed RSDP/XSDT/FADT/DSDT/SSDT
in image-owned memory after ExitBootServices. Real upstream ACPICA
initializes and AcpiEvaluateObject evaluates two synthetic AML
methods: DSDT T822 returns 0x0822, SSDT S822 returns 0x2345.
Actual firmware DSDT is independently checked by S821 and S816.
**No arbitrary firmware AML is evaluated**.

The disposable single-core OSL keeps interrupts disabled, has a
4-MiB bounded allocation arena, and does not reclaim allocations.
Only five private table ranges may be mapped. SystemMemory/SystemIO/
PCI OperationRegion, physical MMIO/ECAM, interrupts and DMA are
deny-by-default. Unsupported async requests fail explicitly.
This OSL must not be used as a production or SMP service.

## Acceptance
Requires actual QEMU/OVMF: disabled-IOMMU, Intel VT-d, one-bridge,
two-bridge and AMD-Vi where available, debug-exit 33, original UART,
both AML returned values, parent PCI/MCFG/IOMMU evidence, negative
UART mutation tests, complete exact-head GitHub CI SUCCESS, full
source ZIP, normal ISO unchanged, original SHA-256.

## Unresolved
Real firmware _CRS/_SEG/_BBN/_CBA and hostbridge ownership;
real ECAM reservation and safe hardware OperationRegion;
IOMMU requester alias ownership, hardware DMA deny/allow/fault/
invalidation; UEFI runtime, production CR3; integrated normal
kernel+Desktop+Processes+native .tea+StyleFS acceptance.
**Foundation NOT COMPLETE.**
