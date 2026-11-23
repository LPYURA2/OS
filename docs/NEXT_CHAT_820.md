# StyleOS — continuation after 820

Previous accepted S819 f0a00c7a12f9bcf9b65dea5b711b29cc9958c370.
S820 task branch feature/styleos-acpica-upstream-abi-820.

Before acceptance verify exact-head full CI SUCCESS, genuine ACPICA 20260930
package SHA-256 and real COFF objects, source ZIP/ordinary ISO hashes,
original QEMU UART/JSON, host and negative tests.

S820 verifies and compiles genuine upstream ACPICA source and checks
the exported 819 callbacks against the official public ABI. **The
actual interpreter is NOT linked into the StyleOS guest**; no AML
execution, OperationRegion permissions, DMA grants or production CR3.

Next stage must finish full native ACPICA interpreter linkage and a
strict freestanding OSL (table, memory, synchronization, operation region).
Then real host-bridge resources, IOMMU requester ownership, DMA domain
hardware deny/allow/faults/invalidation, UEFI runtime and production CR3.
Only after combined ordinary kernel+Desktop+Processes+.tea+StyleFS
hardware acceptance call the foundation complete.

Then full Processes, independent .tea, users Welcome/Lock, StyleFS/Explorer
with «Быстрый доступ», installer, App World, network/browser/audio/VPN.
Never change main, accepted branches, Desktop colors or approved mockups.
Each stage new branch and exact CI, full ZIP/ISO/evidence/SHA/report/handoff.
