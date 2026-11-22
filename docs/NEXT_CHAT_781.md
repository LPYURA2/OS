# StyleOS — continuation point after stage 781

**Project:** LPYURA2/OS.

Stage 781 branch: `feature/styleos-descriptor-provenance-781`.
Exact final commit and GitHub Actions must be taken from the completed CI
run; do not assume success without checking the actual run and artifact.

## Delivered in 781

`kernel/descriptor_state_781.c` and `include/styleos/descriptor_state_781.h`:
read-only bounded IDT descriptor and current boot-stack examination. Analyzes
256 actual IDT gates and bounds CS and TR against GDT, reports IST use and
TR=0 no TSS. A nonzero TR **does not** grant TSS mapping evidence.
Instrumented only the dedicated `--test-frame-grants` x86 QEMU profile, after
real old CR3 restoration and verification of GDT, IDT, and boot stack pages.
Output `STYLEOS DSI 781 INHERITED` is checked against real GDT/IDT UART and
UEFI MEMMAP STACK bounds, including negative/tamper tests.

Ordinary Desktop is unchanged. No new CR3 activation, TSS installation,
page ownership transfer or fabricated independent `.tea` window. Read
`docs/architecture/DESCRIPTOR_STACK_PROVENANCE_781.md` and reports 762–780.

## Next stage — 782

Prepare an actually reserved, page-owned, physically mapped, isolated
TSS/RSP0/IST test context under an experiment-only profile. Validate all
stack pages and descriptor mapping before LTR; protect against nested faults,
stack overflows, stale pointers and invalid hardware modes. Never replace the
ordinary Desktop's descriptor context before a real QEMU positive/negative
proof and restoration. The absence of TSS in the UEFI experiment does not
mean the Desktop already has TSS.

**Later:** full kernel and device memory closure with true page-granular
W^X and guard stacks -> safe own CR3 -> genuine `.tea` processes alongside
ordinary Desktop -> Processes -> login / users by approved mockups -> ACL and
home folders in StyleFS -> Explorer -> transactional `.tea` installer -> App
World. Sound and network later through kernel drivers, independent of each
other.

**Non-negotiable:** each stage new GitHub branch, never change `main`/accepted
builds; preserve existing rich Desktop; never claim a diagnostic root is a
fully working kernel address space; verify QEMU and full CI. Provide full
CI source ZIP, actual bootable ISO, test evidence ZIP, complete bundle and
this handoff for the next chat.
