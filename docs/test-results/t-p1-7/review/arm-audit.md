# T-P1.7 independent AArch64 audit

PASS — no open acceptance blocker found in the inspected ARM path. This is a read-only source audit; no test, QEMU run, repository edit, Git mutation or remote operation was performed. Root owns final integration, factual runtime results and publication.

## Actual exception context and CPU mode

`up_arm_enter` saves only the trusted AAPCS64 dispatcher's nonvolatile call state on the original kernel stack. It independently loads the full user context: X0–X30, SP_EL0, ELR_EL1 and SPSR_EL1. The saved user context is not the T-P1.6 call-boundary context.

The lower-AArch64 synchronous vector is slot8 (VBAR+0x400). It allocates a 320-byte, 16-byte-aligned frame on the separate admitted SP_EL1 stack and saves all31 GPR before using any of them for MRS or C-call arguments. Actual SP_EL0/ELR/SPSR/ESR/FAR/TTBR0 are captured. Raw-frame offsets agree with the C static assertions.

`up_arch_run` restricts resumed SPSR to NZCV and DAIF, with all four masks set and every mode bit clear: this is EL0t. Capture independently repeats that restriction, verifies slot8, exact entry-stack-top frame placement and actual active TTBR0 for the selected slot. Current-EL and unsupported vectors use terminal early exception diagnostics; they do not become a successful user FAULT.

SVC#1/#2 are the only successful operations. Capture checks the hardware syndrome against X0; the common demo checks the exact ESR and PC. The intended LDR produces lower-EL Data Abort ESR0x92000007: AArch64, IL=1, read, FnV=0, not a table-walk/external/permission failure, level3 translation fault. The common demo additionally requires A, the expected fault PC and FAR=0x12010000. Faulted A is never ERET-resumed.

## MMU and isolation closure

Each root has a private finite32-page arena inside the reserved image. Table parents have only address|3; blocks, unknown bits, foreign table pointers, cycles, shared child aliases, unvisited used pages and nonzero unused pages are rejected. The full reachable leaf set is compared exactly to the declared mappings and count.

Kernel image identity leaves are AP00 (EL1RW, noEL0), UXN; the image remains EL1RWX under the documented limited experiment. PL011 is a separate EL1-only Device leaf with PXN/UXN. User code has AP11 (read-only atEL0/EL1), PXN and executableEL0. Private data/stack have AP01 (EL0RW), PXN/UXN. Only the selected program's one code, one data and one stack page are EL0-accessible; no foreign private PA or privileged object receives a user alias. The supervisor image alias of a user code physical page does not make its user mapping writable.

TCR uses a48-bit TTBR0 range with 4KiB granules; TTBR1 is disabled and set tozero. IPS is derived from accepted physical-width capabilities. Image/header, MMU-off initial mode, EL1h/SPSel1, masks and alignment are checked before activation. Every switch uses DSB/ISB and VMALLE1 invalidation. SAFE activation and return read actual hardware root/control state.

## Admission and return path

The common preflight validates whole intervals and excludes the original stack, handoff, map source/workspace and boot pool/metadata before architecture initialization. ARM verifies its fixed Image/header, RESERVED/non-CANDIDATE coverage, vectors and complete new storage, then builds the tables. The physical-address interpretation remains limited to the checked fixed Image with initial MMUoff.

The exception handler captures user data into protected bookkeeping, installs SAFE (which has no user leaves), reads it back, and clears active/context/event pointers. Only then does assembly restore the original dispatcherSP/nonvolatile context and return. Common validates its originalSP/cookie, performs the terminal A/B state transitions and releases only bookkeeping under SAFE. All physical pages remain image-reserved.

B checks its stack local, X19/X20/X21, volatileX9 and private DATA_BEFORE after resume, then stores DATA_AFTER and executes SVC#2. Its linked program and final hardware context must be bound to the actual runtime report by root's integration tests.

## Nonblocking hardening observation

ARM's runtime object-admission loop does not separately list old/new vector intervals among per-object exclusions. Their actual disjointness from writable BSS is established by the linked-section/object proof, while the complete vector spans are admitted separately. Adding explicit vector interval exclusions would make runtime admission more self-contained, but no actual overlap exists in the inspected layout and this is not an acceptance blocker. The common admission already excludes boot pool/metadata.

## Limits

This audit does not establish actual QEMU execution, production process security, arbitrary user programs, FP/SIMD context, preemption, IRQ/SMP, general PMM/heap/reclaim or kernel-fault recovery. The result must remain confined to the new profile, with unchanged older artifacts and truthful runtime evidence.
