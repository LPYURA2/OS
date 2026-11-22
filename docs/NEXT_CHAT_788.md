# StyleOS — next-chat continuation after stage 788

Repo `LPYURA2/OS`. **Accepted base stage 787**:
`8b8ea23acd7718c85eb20053c1ce5b4b6237545c` and full CI
`45032742801` successful. New stage 788 branch:
`feature/styleos-hardware-double-fault-788`.

**Before declaring 788 accepted**, check exact branch HEAD, full GitHub Actions
run and actual `double-fault-788-qemu/serial.log` + `results.json` with genuine
hardware exit 33. Do not infer success from compile or synthetic fixture.

Stage 788 adds only an isolated, terminal x86-64 QEMU/OVMF `#DF` experiment:
`INT3` -> IST1 -> real unmapped guard write `#PF` -> intentionally nonpresent
`#PF` gate -> `#DF` delivered on independent IST3. The handler validates
CR2, zero error code, expected RIP, previous and active RSP, live CR3/TR,
TSS/IDT/GDT and guard PTEs, exits QEMU; no UEFI return. It does not
simulate actual stack exhaustion or prove recovery and does not change
normal Desktop. No production W^X or full inherited-page closure yet.

After 788, prioritize dependency closure for a truly independent and safe
kernel root/stack, production exceptions, and integrated native `.tea`
processes. Do not promise a fixed number of stages; track remaining blockers.

Owner-approved order after foundation: **Процессы**, independent `.tea` from
Desktop, users and login per `StyleOS-Welcome-and-Lock-Screens.pdf`, StyleFS
and Explorer per `StyleOS-Explorer-Mockups.pdf` (`Быстрый доступ`), package
installer, App World, network/audio/VPN. Preserve the polished Desktop and
dark palette; no improvisational redesigns. See
`docs/design/APPROVED_UI_REFERENCES.md`. No changes to main, accepted branches,
Portable-os or other projects.

Every stage: exact SHA and verified Actions, complete CI source ZIP, CI ISO,
hardware evidence archive, docs NEXT_CHAT and **separate ready-to-copy new-chat
message**.
