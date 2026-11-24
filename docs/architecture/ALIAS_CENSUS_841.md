# S841 — inherited table path census

Accepted S840 b11d18dfec1c38faf7b54f847c1c068aa6d4d037, CI 45423685392 SUCCESS (188 PASS, 5 SKIPPED). Independent artifact SHA, source/EFI/ISO hashes, original UART/QMP/EFI replay, COFF and 14 counterfeit refusals verified. Intel three modes each 1 page; AMD 4; disabled 0.

Separate --test-alias-census-841 profile adds bounded recursive table-path enumeration after S840 in the same QEMU boot. Only full safe S840-admitted RAM pages; no target access. 64 admitted paths maximum; explicit denied paths, all 512 entries per admitted path, repeated paths retained for alias detection. Independent host reconstruction records intersecting virtual mappings; partial does not establish global absence. Scope is current inherited four-level root, not all possible roots or production. No DMA, allocation, mapping mutation or activation. Full CI and negative mutation tests still required before acceptance. Foundation incomplete.

Pre-acceptance revision: bind CPU fields to the S837 snapshot, require the 1GiB feature for such leaves, validate identity coverage page-by-page and run mandatory negative mutations on the genuine S841 captures. Results remain pending until CI and independent archive replay.

S841 initial CI 45432687247 failed at EFI link: `scan841` allocated a 4096-byte local table array and introduced undefined `__chkstk`. Correction: a bounded 16-entry stack row and depth-first row/child replay; no new stack-probe stub, no disabled compiler stack probing. Local Clang COFF build PASS, no `__chkstk` symbol, observed scan841 stack subtraction 0x128 plus saved registers. New mandatory symbol check in CI. Guest result and final link pending. Original failed run retained.
