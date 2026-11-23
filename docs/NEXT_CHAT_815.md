# StyleOS — exact handoff from 815

Accepted parent 814: `d326a035cc5fb069703bf132db0d3bb20636b219`.
Task branch 815: `feature/styleos-acpi-static-namespace-815`.

New isolated x86-64 terminal option `--test-acpi-hostbridge-815` extends
the 814 UEFI/MCFG/PCI observation with read-only FADT/DSDT verification,
bounded static AML candidates and reassembled original DSDT UART evidence.

Before accepting: confirm GitHub Actions SUCCESS for the **exact final
commit** and inspect the source ZIP, ordinary ISO hash against 814,
all QEMU raw UART, original DSDTs, host/negative logs and JSON. A local
parser test or previous intermediate run does not mean full acceptance.

**Not proven:** executed ACPI `_CRS`, `_SEG`, `_BBN`/`_CBA`,
motherboard ECAM reservation, arbitrary PCI segments, ownership of
IOMMU controller, real DMA domains/deny/allow, faults, invalidations,
UEFI runtime or production CR3. No DMA authorization.

Next stage in a *different* branch: constrained ACPI AML namespace
interpreter with SSDT loading, resource and OperationRegion policy,
then real ownership/alias verification. Afterward hardware DMA deny/allow,
UEFI runtime, production CR3 and full ordinary kernel/Desktop/Processes/
native .tea/StyleFS file operations integration. Only after integrated
hardware acceptance may the foundation be called complete.

Then: full Processes PID/CPU/RAM/protected critical processes and tabs;
independent .tea; users/Welcome/Lock per PDF; StyleFS/Explorer per PDF
with «Быстрый доступ»; .tea installer, App World, network/browser/audio/VPN.

Preserve `main`, all accepted branches, Desktop/palette/mockups,
LPYURA2/Portable-os and history. No merges, releases or force pushes.
Deliver exact SHA, full successful CI ZIP, ISO, verified hardware logs,
technical report, continuation point and next-chat message every stage.

**Foundation NOT COMPLETE.**
