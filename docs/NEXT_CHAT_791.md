# StyleOS continuation after 791

Repo `LPYURA2/OS`; base accepted 790 `2885ef6da34bc00693e0d61e2bdd80b0bdde4bad`.
New 791 branch `feature/styleos-hardware-wx-pf-791`. Before acceptance fetch
exact final commit SHA and a FULL successful GitHub Actions run with BOTH
independent real QEMU `wx-write-791-qemu` and `wx-exec-791-qemu` passes.

791 tests CPU enforcement of supervisor RX write rejection (CR0.WP=1, PFEC=3)
and NX execution rejection (EFER.NXE=1, PFEC=0x11) with genuine #PF on
separate IST2. It uses terminal independent low PDPT/PD/PT and physical IST
aliases. It DOES NOT claim production W^X, device closure or independent .tea
co-execution. Never return to firmware after terminal CR3/LTR.

Next: inventory actual kernel/device/IRQ/GOP/MMIO dependencies, verify owned
page-table closure of the ordinary Desktop before any production CR3 switch.
No user-visible UI changes or mockup alterations in kernel experiment.

Approved plan: finish foundation -> full Processes app real PID/CPU/RAM and
control -> shell-independent .tea apps -> users/login/lock exactly per
`StyleOS-Welcome-and-Lock-Screens.pdf` -> StyleFS rights and Explorer exactly
per `StyleOS-Explorer-Mockups.pdf` ("Быстрый доступ", no "Рекомендованное")
-> installer .tea -> App World -> network/browser/audio/VPN.

New stage = new branch; preserve main and accepted branches and polished
Desktop. Deliver exact SHA/run, real CI ISO, full source ZIP, hardware evidence,
architecture report, NEXT_CHAT and ready-to-copy MESSAGE_FOR_NEW_CHAT.
