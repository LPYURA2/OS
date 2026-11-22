# Stage 782: image-owned TSS/RSP0/IST candidate (not active)

The hardware QEMU grant profile now reserves **four separate 4 KiB PE-image
pages**: candidate GDT, candidate TSS, RSP0 stack, and IST1 stack. The exact
physical extents must be inside the UEFI LoadedImage reserved range; be
disjoint from the previous 79 granted pages, live stack, input, metadata,
and candidate PMM memory; and individually pass the inherited CR3 page walk
(supervisor, identity PA, writable, WB).

The host sanitizer suite checks negative cases (alias, candidate overlap,
misalignment, missing reservation, refused hardware attestation) and confirms
that failure never produces a ready plan or alters a TSS buffer. Once
attested, a 104-byte x86-64 TSS **data image** can be encoded with RSP0 and
IST1 tops pointing to the dedicated reserved stacks. A candidate GDT page is
reserved but the GDT and TSS CPU descriptors are not installed.

No LTR, no LGDT or LIDT, no interrupt-stack selection, no guard PTEs,
no IRQ nesting test and no new CR3 activation happen in 782. Inherited UEFI
pages still use permissive RWX policies: the TSS image is **not** proof of
a completed W^X kernel mapping. Ordinary desktop is byte-identical to 781
when built from the same source tree. TSS/IST should not be reported as
functional in regular StyleOS until actual hardware install, exception,
recovery and guard tests pass.

A real QEMU proof prints four STYLEOS DSI 782 FRAME PA/VA lines and a
STYLEOS DSI 782 STAGED line declaring ltr=0 and guard=unproven.
Its parser checks each 4 KiB physical page against the UEFI image range and
inherited CR3 plus negative tamper checks. Source and CI pin the exact commit.

Next: 783 should securely construct and attest the candidate GDT/TSS descriptor
pages and emergency stack guard mappings before a **separate**, narrowly
contained hardware LTR test. Preserve the inherited descriptor context
until rollback is demonstrated.
