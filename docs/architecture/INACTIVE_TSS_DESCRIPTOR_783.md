# Stage 783: inactive, hardware-attested GDT/TSS descriptor

The dedicated QEMU frame-grant profile already attests the four image-owned
pages of stage 782 (GDT, TSS, RSP0, IST1) and the live firmware GDT/IDT and
boot stack (781). Stage 783 builds a fresh **non-active** 4096-byte GDT from
the bounded inherited GDT and appends one **available** 16-byte long-mode
TSS descriptor (type=0x9, present, ring0, limit=103) at selector
`old_gdt_limit+1`. The TSS base is the individually attested page. The
104-byte TSS holds the 782 RSP0 and IST1 stack tops with the I/O map beyond
limit. Original GDT entries and the current CS selector are preserved.

The pure builder validates all inputs before writing any GDT byte. Host
ASAN/UBSAN tests reject malformed CS, TR, misalignment, corrupt TSS,
bad bounds, aliases and forged activation. The independent QEMU UART
validator cross-checks GDT/CS/CR3 against the older hardware proofs and
checks actual selector/limit/TSS base and stack top arithmetic.

**No LGDT, LTR, IDT change, owned CR3, guard PTE or IST trap occurs.**
Firmware's observed TR=0 cannot be safely restored by simply running
LTR(0). The next stage must use a separate *terminal* QEMU experiment,
not a return to firmware or the ordinary Desktop. Image pages remain
inherited RWX; hardware W^X and full kernel device mapping are not ready.
