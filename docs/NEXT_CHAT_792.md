# StyleOS — handoff after stage 792

Base accepted stage 791: `158ca0b6737aa4b985d7e24ceeb623ca3981e655`.
Stage 792 branch: `feature/styleos-production-closure-preflight-792`.
Before acceptance verify the **exact final SHA**, complete GitHub Actions
`verify-and-archive` success, and source/ISO evidence from that run.

792 adds `production_closure_792` (read-only source/hardware-dependency
manifest policy). It enforces safe, bounded physical/IO-port inventory,
expected RX/RW/NX/cache policy and missing/unverified masks. All paths return
`activation_permitted=false`. Host ASAN/UBSAN and x86 COFF checks do not
prove hardware closure. All old independent hardware regressions remain
mandatory in CI. This stage NEVER activates production CR3 or changes Desktop.

Next 793: gather actual GOP/framebuffer/page type/cache evidence, I/O ports,
IRQ and firmware-runtime lifecycle, then plan an independently tested
production mapping-closure protocol. Do not claim hardware pass for 792
manifest itself. Do not change main or previously accepted branches.

Long-term fixed order: foundation -> full real `Процессы` -> independent
native `.tea` alongside Desktop -> users/login/lock per
`StyleOS-Welcome-and-Lock-Screens.pdf` -> StyleFS and Explorer per
`StyleOS-Explorer-Mockups.pdf` (`Быстрый доступ`, no `Рекомендованное`) ->
installer .tea / App World / network / browser / audio / VPN.

Per-stage deliverables: full source ZIP from successful CI, ISO, evidence,
exact SHA / Actions link, technical report, continuation file, and a separate
ready-to-copy new-chat message. Keep polished Desktop and palette intact.
