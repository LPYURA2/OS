# S841 — inherited table path census

Accepted S840 b11d18dfec1c38faf7b54f847c1c068aa6d4d037, CI 45423685392 SUCCESS (188 PASS, 5 SKIPPED). Independent artifact SHA, source/EFI/ISO hashes, original UART/QMP/EFI replay, COFF and 14 counterfeit refusals verified. Intel three modes each 1 page; AMD 4; disabled 0.

Separate --test-alias-census-841 profile adds bounded recursive table-path enumeration after S840 in the same QEMU boot. Only full safe S840-admitted RAM pages; no target access. 64 admitted paths maximum; explicit denied paths, all 512 entries per admitted path, repeated paths retained for alias detection. Independent host reconstruction records intersecting virtual mappings; partial does not establish global absence. Scope is current inherited four-level root, not all possible roots or production. No DMA, allocation, mapping mutation or activation. Full CI and negative mutation tests still required before acceptance. Foundation incomplete.
