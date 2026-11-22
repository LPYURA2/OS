# StyleOS stage 801 — live inherited paging and PAT/PTE evidence

Starting point: accepted 800 commit `6e2635c98dd5e62c9cd4461cdf5da6ac42a05fde`.

**Scope:** separate, terminal QEMU/OVMF x86-64 firmware-CR3 diagnostic. It reads
active CR3 and IA32_PAT, walks 4-level paging for three *numerical sample virtual
addresses* corresponding to observed GOP and VirtIO BAR bases. It distinguishes
present/absent leaf PTEs, 4K/2M/1G leaves, page-translation physical output,
PAT index and selected PAT-entry type. It never accesses MMIO content, updates
CR3/PAT/MTRR/PCI or uses UEFI boot-service calls after ExitBootServices.

The read callback requires page table physical pages below the QEMU 128MiB RAM
limit, backed by a non-MMIO UEFI memory descriptor. It relies on *inherited
firmware low-memory identity access* for reading those table pages. This
assumption is not a production kernel mapping API; alternative firmware layouts
may fail closed. The walker is unit-tested without hardware with ASAN/UBSAN,
COFF and multiple leaf/absent/refusal cases. Strict UART parser rejects forged
field/leaf/PAT/policy combinations; success requires a real QEMU debug-exit 33.

**Critical limits:** Sampling a virtual address equal to a device physical BAR
base is not exhaustive inverse alias discovery. The target could be absent or
translate to an unrelated PA, so a sample alone does not prove the MMIO device
is mapped. A PAT byte read from a page-table entry is not full effective
cache type until MTRR, 4-level hierarchy, aliases, large pages, control bits
and ownership are jointly validated. Do not equate physical MTRR UC from 800
with proven MMIO UC for production. `production_cr3_authorized=false`.

No source code for the ordinary Desktop changes semantically; compare the
actual ordinary EFI with the exact previous CI artifact. No production CR3,
no IRQ/APIC, DMA/IOMMU, firmware runtime closure, or independent .tea/Desktop
integration is proven in this stage. Foundation **not finished**.
