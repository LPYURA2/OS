# Independent T-P1.4 review (work in progress)

Scope: read-only source review and separate host checks; the reviewer does not claim these are real QEMU boots. Parent agent owns full target tests and publication. No repository files, Git refs or external services were changed by this reviewer.

## Reviewed design

- Dedicated 32-page table reserve plus separate control/data pages exists only in the two paging profiles. Admission reuses successful diagnostic evidence and full image reservation, excludes stack/handoff/map-source/workspace and both existing pool objects, and checks complete object extents before control writes.
- Generic constructor builds four-level 4-KiB leaves from explicit VA/PA mappings, refuses invalid bounds/width/alignment/overlap/type/right/probe/capacity plans, and never returns a usable root after failure. Child descriptor PAs resolve only through the supplied image-owned table array.
- Page-level image closure is intentionally RWX and supervisor-only. This is a bounded experimental mapping, not W^X or a production ownership contract. The data alias preserves write/type and drops execution; it does not acquire RAM.
- ARM uses explicit lower 48-bit stage-1, 4-KiB granule, upper range disabled, normal NC image/data, separate Device-nGnRnE PL011 page, masked interrupts and unchanged VBAR/startup. MAIR/TCR/TTBR/SCTLR and barriers are checked; caches remain disabled.
- x86 validates four-level long mode/width/NX/PAT and rejects PCID/LA57/CET/PKS and unmodeled conditions. It translates retained GDT VA through inherited tables, checks active CS/IDT, proves table reserve physical correspondence, flushes inherited translations and verifies new CR3. Physical old-table reads are restricted to the supported UEFI boot contract and checked identity closure; this is not a generic pointer-to-PA cast.
- Static ELF/PE proof checks complete paging/pool object intervals, BSS/virtual-tail/PT_LOAD/Image coverage, initialized data and actual managed read instruction. Caller rederives it from current artifacts and compares hashes.

## Closed review findings / recommendations

1. Retained read-only GDT must already have active CS Accessed=1; otherwise interrupt delivery could attempt a descriptor write. Architecture agent added descriptor checks.
2. GDT numeric bounds must be validated and old translations checked before reading descriptor bytes. Ordering fixed.
3. Writable identity proof must cover all 32 table pages and control/data pages, not only the first table page. Full-span proof added.
4. CR0.WP and PAT slot0 WB must be accounted for; unrelated CET/PKS dependencies rejected. Activation establishes and reads back WP. Old CR3/nonleaf PWT/PCD profiles were narrowed to avoid mixed cacheability proof.
5. A corrupt probe descriptor must not count as unmapped merely because lookup returns some error. Root now requires exact PG_RANGE for a missing entry.
6. UART parser must not accept a GDT inside image RWX or a read-only physical alias of writable image RAM. Generic PA-alias and GDT exclusion checks added.

## Actual independent host checks so far

- GCC ASan+UBSan: generic constructor/lookup host suite PASS (`review-host/core-sanitize.log`).
- GCC ASan+UBSan: paging admission host suite PASS (`review-host/admission-sanitize.log`).
- GCC ASan+UBSan: pure x86 mode/descriptor/inherited-walk checks PASS (`review-host/x86-sanitize.log`). No privileged instruction executes in this host build.
- Paging ELF/PE crafted-fixture suite PASS 12/12 (`review-host/site-tests.log`).

## Pending

Final UART/parser review, real six-profile QEMU logs, complete regressions, independent ARM-only route and final report consistency remain parent-agent work. Initial integration x86 correctly refused an unsupported/unproved platform condition without switching and is being diagnosed; no x86 success is claimed here yet. Android/Termux T-P1.4, VirtualBox and physical hardware are not covered by this review.
