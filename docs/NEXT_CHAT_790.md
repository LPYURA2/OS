# StyleOS — continuation after stage 790

Repository `LPYURA2/OS`. Base accepted 789:
`6914ae2c3e83bce437f297a7f53ea9168eb3725a`, Actions `45035821747`
SUCCESS. Stage 790 branch `feature/styleos-owned-low-tables-wx-790`.

Before accepting 790: verify exact final SHA; full successful GitHub Actions;
real QEMU `owned-low-790-qemu/serial.log` and `results.json` with debug-exit
33; ASAN/UBSAN host tests and strict UART negative checks. Do not claim
hardware PASS based on local compilation alone.

Stage 790 adds terminal-only own PDPT/PD/PT beneath PML4[0], with 4KiB PE
section based RX/RW+NX/RO+NX identity pages and boot stack RW+NX, plus own
IST1/2/3 alias at slot 128. No low firmware PDPT/PT retained after CR3.
It repeats actual INT3/#PF/#DF nested hardware chain under the new root.
Normal Desktop is unchanged; no production W^X, IRQ/driver/GOP closure or
independent native .tea co-execution claim. See
`docs/architecture/OWNED_LOW_PAGE_TABLES_WX_790.md`.

Next 791: in a SEPARATE terminal experiment, prove physical CR0.WP and EFER.NXE
policies by two distinct negative probes on actual code RX and data RW+NX,
with precise hardware #PF error and CR2/RIP. No unverified shortcut or
return to UEFI. Then move toward full production critical image/device
closure, and real shell-independent .tea co-execution.

Owner sequence after foundation: complete «Процессы» with real PID/RAM/CPU
and control; independent .tea app processes; users/login/lock following
`StyleOS-Welcome-and-Lock-Screens.pdf`; StyleFS rights and Explorer following
`StyleOS-Explorer-Mockups.pdf` («Быстрый доступ»); app installer, App World;
then network, sound and VPN. Preserve existing Desktop and palette.

Every stage new task branch. Do NOT edit main, accepted branches or other
projects; no merge or release without owner authorization. Every stage
must deliver full source ZIP, actual CI ISO, QEMU evidence, SHA/run URL,
architecture report, NEXT_CHAT and ready-to-copy MESSAGE_FOR_NEW_CHAT.
