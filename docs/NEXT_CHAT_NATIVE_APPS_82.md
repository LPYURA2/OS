# Stage 82 — coalesced dirty-cell scaling candidate

Base 8a44b118a5cf00f78d4c254d3bb71e35edfc15de. Stage 80 20/20 PASS. Stage 81.2 actual guest patch job PASS, 41 CPL3 events and screenshot checks, full regression pending.
Provisional cache expands from 4 to 16 cells/port (~16.3 KiB/port for cells, ~130.5 KiB across eight ports). No unbounded allocation. Each cell records latest generation, base frame generation gates incremental damage across multiple submissions, repeated cell writes coalesce. Unclipped window bounds prevent false unchanged geometry under clipping. Full replacement/geometry changes retain conservative repaint. Extended pixel/ASAN/COFF tests and all previous guest jobs. No CI PASS for 82 yet; still not full-scale surfaces or shared pooled graphics memory. Next use a scalable bounded representation for rich text/UI without huge per-port arrays (retained text/tiles or pooled backing), real guest coverage, then scoped file API. Preserve all refs, Astra work and main; no product UI until owner supplies mockups. Beta 4 GiB RAM/20 GB disk, idle <1 GiB goal unmeasured.

## Observed results
Stage 82 CI 46130513032: 19/21 successful including patch compositor host and actual CPL3 patch guest; ordinary shell and StyleFS still running. Stage 81.2 final retry CI 46130366925: 20/21 successful at observation, StyleFS pending. Do not claim full acceptance yet.
