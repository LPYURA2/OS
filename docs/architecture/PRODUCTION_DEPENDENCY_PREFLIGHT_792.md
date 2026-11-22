# StyleOS stage 792 — production dependency closure: read-only preflight

## Accepted scope and hard limits

Stage 791 proved hardware RX write rejection and NX fetch rejection **only** in
isolated terminal x86 QEMU tests. Stage 792 does **not** switch production CR3,
modify Desktop's page tables, disable IRQs or declare general kernel isolation.
It introduces a read-only resource inventory and a conservative policy checker.
`pc792_inventory` **always** returns `activation_permitted=false`, even if all
caller-supplied evidence is marked `ATTESTED`. That marking is not itself a
hardware attestation. A future separate hardware-bound protocol must verify
physical ownership and device cache policy before authorizing any switch.

## Grounded inventory, current ordinary x86 Desktop

| Dependency | Source anchor | Evidence today | Required future proof |
| --- | --- | --- | --- |
| Loaded PE executable/RO/RW segments | `kernel/kernel_dependency_inventory.c`, `boot/x86_64/uefi.c` | Hardware terminal experiments 790/791; not Desktop activation | Exact loaded-image physical span and PE relocations, W^X across entire live image |
| Current ordinary stack and task stacks | `arch/x86_64/paging.c`, `arch/x86_64/tasks.c` | Historical inherited walk; ordinary Desktop closure not proved | Each live stack including dispatch, task and exception stacks, exact ownership and guards |
| GDT, IDT, TSS and IST | `kernel/descriptor_state_781.c`, `kernel/ist_hw_trial_786.c` | Terminal 784–788 verified, not production | Live descriptors and every gate's stack and selector under new CR3 |
| Page-table allocation and aliases | `arch/x86_64/paging.c`, `kernel/owned_low_trial_790.c` | Separate terminal tables only | Physical provenance, A/D mutation, no dangerous alias or reserved-RAM overlap |
| UEFI boot memory descriptor map | `include/styleos/boot_info.h`, `boot/x86_64/exit_boot.c` | v1 memory map provided after ExitBootServices | Lifecycle/ownership and copy of descriptor buffer, no reuse or reclaimed firmware pages |
| GOP framebuffer | `boot/x86_64/desktop_display.c`, `kernel/desktop_display.c` | Boot adapter copies GOP mode; drawing uses `volatile` writes | Page-covering framebuffer span, WC/UC cache policy, PAT/MTRR coherence, validated exclusions |
| Serial / keyboard / ATA port I/O | `arch/x86_64/serial.c`, `arch/x86_64/desktop_io.c` | Direct `inb/outb/inw/outw` (I/O ports), not MMIO | Enumerated I/O-port permissions; distinguish these from memory-map MMIO |
| Other device MMIO and caches | `arch/x86_64/ownership.c`, `arch/x86_64/paging.c` | Not fully enumerated in ordinary Desktop | Enumerate BARs/MMIO and cache policies; no blanket identity map |
| IRQ and trap state | `arch/x86_64/exceptions.c`, `arch/x86_64/tasks.c` | Terminal faults proved, full Desktop interrupt closure unresolved | Every live vector/handler, interrupt-mask state and safe exit/reentry |
| Firmware/runtime lifecycle | `boot/x86_64/exit_boot.c`, `boot/x86_64/uefi.c` | ExitBootServices path exists; runtime mapping policy unresolved | Explicit proof that firmware runtime services are not used, or exact runtime mappings |

`boot_info v1` contains no framebuffer base/size fields; the Desktop GOP
adapter passes data by a separate boot-only path. Never extend `boot_info v1`
without a separate approved ABI change. The fact that the framebuffer address
appears as a pointer is **not** proof of its actual PAT/MTRR cache type.

## New API and denial tests

`include/styleos/production_closure_792.h` and
`kernel/production_closure_792.c` define 13 bounded dependency roles,
physical/IO-port/assertion resource classes, WB/WC/UC policies, expected
RX/RO/RW access rights, duplicate and overlap rejection, and a non-activation
report with missing and unverified masks. The layout check intentionally
requires page-aligned, page-covered device ranges; a raw, potentially
unaligned GOP aperture must be normalized with explicit bounds before a future
caller can submit it. It does not inspect hardware, map pages, or verify
whether a caller's `ATTESTED` label is truthful.

Host checks compile the freestanding C file with ASAN/UBSAN and COFF. They
reject mixed W+X, user-accessible supervisor regions, missing/unknown cache,
overlap, malformed I/O ports and spoofed resource classification. Even a
synthetic, all-ATTESTED manifest remains activation-denied. A source-anchor
report documents exactly which repository files were inspected; anchors are
not hardware proof, and changes require re-auditing.

## Next stage 793

Gather a **real observed** framebuffer physical span/descriptor type and
cache-policy evidence, plus actual IO-port and runtime lifecycle inventory,
without modifying the ABI or the current Desktop. Then separately attest
address-space closure under controlled QEMU/OVMF with precise rollback and
negative tests. Only a separately accepted integration stage may touch
production `CR3` or Desktop device mappings.

Plan retained: foundation -> full `Процессы` -> independent `.tea` ->
user login/selection -> StyleFS/Explorer, strictly following accepted mockups.
