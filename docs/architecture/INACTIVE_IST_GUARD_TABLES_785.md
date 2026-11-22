# StyleOS 785 — inactive IST guard page-table recipe

## Base / scope

Previous **accepted** revision 784: `7e36480681dd9045a9929a13be072d194d57adea`,
Actions run `45026149638` SUCCESS (dedicated terminal LGDT/LTR/STR hardware test).
New branch: `feature/styleos-ist-guard-table-plan-785` from **exact** 784 commit.

Stage 785 is a deliberately separate **software-only** preparation of a guarded
IST stack alias; **no CR3/IDT/TSS hardware writes** and no hardware guard or IST
entry claim. 784 proved *installation of a TSS*, not delivery of an exception on
an IST stack. An inactive plan is not a protected runtime stack.

## Layout / strict invariants

Nine distinct 4096-byte image-backed/reserved candidate pages are described by
physical/identity addresses: new PML4, PDPT, PD, PT, IDT, GDT, TSS, RSP0, IST1.
A real guest must individually attest these via full old-CR3 walk and boot map;
this host admission does not assert ownership or make generic PA claims.

The proposed IST1 alias is `0x0000400000003000`, with unmapped neighbors
`0x0000400000002000` and `0x0000400000004000`. PML4 index 128, PDPT index 0,
PD index 0, PT index 3. The proposed leaf contains only P|RW|NX and **no US**
(`0x8000000000000003` plus the IST1 physical frame address). PT[2] and PT[4]
remain exactly zero; the 509 other leaves are also zero. PDPT, PD and PT have
exactly one present supervisor entry each.

The PML4 is a copied **inherited** root, changing only one slot (128). Any
nonzero inherited root slot 128 is an unambiguous refusal, including a stale
non-present entry. Neither the original root bytes nor its mappings are edited.
Using inherited mappings elsewhere is *not* proof of safe production W^X,
firmware/IO closure, or independence from UEFI. NXE support and all expected
hardware addresses must be proven by a later real-CPU validation.

Two negative-purpose functions are provided:
`igp785_prepare_tables()` refuses alias occupancy and clears its outputs on
failure; `igp785_audit_tables()` rejects nonzero guard PTEs, user/executable
stack aliases, any unexpected table entry and unsupported activation flags.
All published flags remain `structural_only=true`,
`hardware_guard_proven=false`, `hardware_ist_proven=false`,
`activation_allowed=false`.

## Acceptance / tests

- Host ASAN/UBSAN policy refusal tests; COFF cross-compilation for x86_64.
- Strict deterministic page-table recipe and independent byte-by-byte audit
  with deliberate guard PTE, US, NX, active-bit and PML4 occupancy mutations.
- Full inherited CI host, desktop/QEMU, 784 terminal LGDT/LTR and all other
  regressions. `main`, 784, the ordinary Desktop and graphics not edited.
- Only accept 785 structural work when the **exact** final commit completes CI.
  CI PASS does **not** mean hardware IST entry or guard fault handling PASS.

## Unresolved physical proof (next stage)

A separate terminal QEMU/OVMF **786** experiment should:
1. independently attest all nine image-backed pages and verify PML4[128] absent;
2. attest inherited execution, IDT, GDT, full active boot stack and MMIO/UART
   mappings under a cloned root, and set proper reserved/WB/NX permissions;
3. activate cloned CR3, LGDT/LTR, load an IDT with IST1 on a controlled vector;
4. trigger an actual `INT3`/fault and check saved RSP is inside the one mapped
   IST alias; prove guard PTEs nonpresent in active CR3;
5. exit QEMU through a terminal debug port without ever returning to UEFI,
   and independently reject timeout/reset/faults, forged UART or exit codes.

No `LTR(0)` restoration, no guard-page intentional overflow in this stage,
no claim of surviving stack exhaustion or nested double faults. Later:
independent `.tea` + Desktop, Processes, login/user switching, StyleFS v2 rights
and Explorer strictly per approved design references.
