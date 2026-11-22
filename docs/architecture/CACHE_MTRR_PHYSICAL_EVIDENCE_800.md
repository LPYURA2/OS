# StyleOS 800 — conservative PAT/MTRR physical-cache evidence

Base: accepted stage 799 commit `958dfabb229d6718da0badf4f8a67092db48c946`.
New branch: `feature/styleos-cache-evidence-800`.

### What was actually implemented

A separate x86-64 UEFI diagnostic image (`--test-cache-800`) performs a
**read-only**, single-CPU observation *after ExitBootServices* under the
inherited UEFI CR3. It obtains hardware CPU `CR0`, `CR3`, `IA32_PAT`,
`IA32_MTRRCAP`, `IA32_MTRR_DEF_TYPE`, CPU physical-address bits and the
variable MTRR base/mask MSR pairs. It reads real PCI BAR1/BAR4 bases of
VirtIO Serial `01:00.0` via CF8/CFC without writing PCI. GOP base comes
from the captured UEFI mode.

The conservative MTRR-domain classifier reports matches and a possible
physical MTRR memory type (0=UC, 1=WC, 4=WT, 5=WP, 6=WB, 255=unknown),
including overlap rules for UC dominance and WB+WT. This does not prove
effective cache selection: that also requires *the translation actually
used for the device* and its PTE PAT/PCD/PWT bits, effective combinations,
MTRR fixed ranges where relevant and correct device ownership/alias rules.
No arbitrary new BAR mappings, device probing, cache-setting writes or
production CR3 activation are performed.

Every UART record is parsed independently; the parser recomputes the MTRR
classification and enforces a final deny policy. The real QEMU runner
requires the exact hardware UART and debug exit 33. Negative/fake logs are
explicitly not hardware proof. ASAN/UBSAN and freestanding COFF checks test
the evaluator separately.

### Scope limits

This test reports snapshots for the current QEMU/OVMF CPU, not another host.
No IRQ/APIC route or DMA/IOMMU mapping is enabled or validated. No UEFI
runtime closure is demonstrated. This stage **does not** authorize
production `CR3` or claim full effective PAT/MTRR cache types. The ordinary
Desktop binary must remain unchanged. Foundation remains incomplete.

Next: prove cache aliases and effective MMIO mapping with page-walk and
hardware access under a separate safety gate; then IRQ/APIC, DMA, UEFI
runtime and finally production Desktop integration testing.
