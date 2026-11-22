# Stage 802 — conservative PAT/MTRR sample correlation

Branch: `feature/styleos-cache-crossboot-correlation-802`. Exact accepted base:
`411cafba47e07f8f25d855f07314b07229051971` (S801).

This stage adds an independent mandatory CI audit over **fresh** results from
S800 and S801's existing, separate, real QEMU/OVMF boots. The validator
reparses both raw UART logs (exit code 33), authenticates each to the
accompanying stage result SHA-256, checks CPU CR0/CR3/PAT/physical width,
PCI/GOP physical device addresses, MTRR UC evidence and identity-mapped leaf
PAT type WB at three sampled addresses. For the three *samples only*, MTRR UC
is dominant over a selected PAT WB type under x86 cache type combination rules.

**Not a single-boot atomic measurement**. Matching register values across
separate QEMU boots do not prove the hardware state cannot change or that
multiple virtual aliases are absent. This is not a general PAT/MTRR cache
driver: it rejects other policies/configurations as unverified, rather than
assuming they are insecure or impossible. It does not touch any hardware,
Desktop, memory managers, firmware mappings or PCI BARs. It never authorizes
production CR3. The underlying S800 and S801 EFI still execute in real QEMU
and are CI requirements; host fixtures alone do not suffice.

Remaining: full alias inventory, sampled *same-boot* effective policy,
PCI/MMIO ownership beyond the measured BARs, IRQ/APIC, DMA/IOMMU and UEFI
runtime. No kernel-owned CR3 integrated with normal Desktop, apps or files.
Foundation remains **NOT COMPLETE**.
