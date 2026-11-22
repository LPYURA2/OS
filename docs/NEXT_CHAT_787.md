# StyleOS — continuation after Stage 787

Stage 787 branch `feature/styleos-hardware-guard-pf-787` from accepted
786 `b6eab7b883694a5a255b16be59eaa443754430f3`, run `45031146443`
SUCCESS. **Check actual final 787 SHA and its Actions job before acceptance.**

New isolated terminal QEMU experiment: hardware `INT3` arrives on IST1,
then deliberate write to the absent IST1 lower guard invokes real #PF on
IST2. Verify CR2, error code 2, actual fault RIP, old/new RSP, active
CR3, TR, A/D and four absent guard PTEs; no return to UEFI. Preserve
Desktop and all accepted branches. The nested-fault path, #DF, stack
exhaustion, production W^X and integrated independent .tea are still NOT
verified. A test passing on a host parser alone is not acceptance.

Next proposed stage: separate #DF fault-safe handler and/or closure of
inherited kernel/firmware/device mappings with guarded supervisor stacks,
not integration into production prior to proof. Do not claim production
readiness while dependencies remain.

Owner-approved order AFTER foundation: full «Процессы» (real kernel PID,
CPU, RAM, per-process state, process controls and three tabs), independent
native `.tea` running without Desktop shell (IPC/window/service lifecycle),
login/user selection by `StyleOS-Welcome-and-Lock-Screens.pdf`, StyleFS
and Explorer by `StyleOS-Explorer-Mockups.pdf` («Быстрый доступ», never
«Рекомендованное»), then package installer, App World, network/audio/VPN.
Preserve the established polished desktop and design, not a generic clone.

Required every stage: CI source ZIP, CI ISO, full hardware evidence archive,
exact commit and Actions URL, NEXT_CHAT document AND standalone ready-to-copy
message for a NEW CHAT. Read AGENTS.md, docs/PROJECT.md and related reports.
