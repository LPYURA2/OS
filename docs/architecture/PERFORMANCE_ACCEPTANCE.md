# StyleOS performance acceptance

Owner requirement: optimization is continuous, not deferred until release. Beta target VM: 4 GiB RAM / 20 GB disk. This is not a measured minimum. Preserve isolation, security checks and durability; do not trade correctness for benchmark scores.

69 initial measurements: ordinary QEMU end-to-end host monotonic launch-to-frame and close times (includes QMP and screenshot overhead), actual guest CPU trace syscall counts, input polls and disk I/O. Compare identical workload, guest resources, hypervisor version, image and host conditions. Report raw metrics, median/p95 with repeated workloads, distinguish host and physical tests; never claim speedup without paired data.

70 gate: record process/resource counts before/after normal exit, fault and repeated launch, exact owned pages and leak-free release; measure input poll rate and idle CPU, event-driven sleep/wakeup before claiming efficient native apps. Existing busy polling is a known optimization target; do not add arbitrary sleeps or weaken scheduler proofs. Check cursor damage budget and overlapping surface updates. Keep filesystem durability and large-file checks. Later performance baseline on actual 4 GiB / 20 GB VM, plus physical Intel/AMD hardware when available. No numerical target for CPU/launch latency until a reproducible baseline exists.

## Beta desktop memory budget — owner goal 2026-11-27

Target: less than 1 GB (1,000,000,000 bytes) of non-reclaimable guest RAM at idle after the normal desktop is ready, with all default system services running and no user applications open. This is an optimization goal, not a measured result or a minimum boot requirement. Keep the Beta VM at 4 GiB RAM / 20 GB disk. Also report MiB and total occupied RAM including reclaimable caches; do not confuse host QEMU RSS with guest RAM.

Proposed measurement contract for stage 70 and subsequent Beta regressions: report usable/managed physical RAM, reserved/unavailable ranges, free pages, kernel image and allocations, desktop/system services, framebuffer/surfaces, process-private and shared pages (count physical pages once), and reclaimable caches separately. Reconcile totals without silently omitting unclassified allocations. Sample after boot, after 60 seconds idle, during a two-app workload, after normal close and after fault/forced termination; repeat a fixed launch/close workload at least 100 times and report retained pages and high-water marks. Establish repeatable baseline before tuning; clearly mark unavailable measurements. Do not treat a fixed VM memory setting as actual consumption.

Use on-demand loading, event-driven waiting, bounded caches, shared immutable pages where supported, and damage-only rendering as candidates subject to profiling and isolation checks. No disabling mandatory services, reducing fault containment, bypassing validation, or concealing memory in caches to meet the target. Review the budget again with all default Beta services installed, then separately validate on VirtualBox and physical hardware.

70 idle instrumentation: actual authenticated input syscalls counted per process generation, aggregate receipts only on successful dequeue. Compare empty requests between consumed pointer release and Q after a 10-second host no-input interval, explicitly including boundary overhead. Current scheduler can skip WAITING but bank validity and forced termination require additional changes before safe event-driven waits; no arbitrary sleep added. Separate full RAM reconciliation and event-driven scheduling work remain required before claiming optimization complete.

Owner direction: reuse proven OS optimization principles from Windows and Linux where compatible, e.g. event-driven application waits, ready queues, lazy loading, bounded caching, page accounting and incremental drawing. Prefer documented interfaces and appropriately licensed reusable implementations; no proprietary Windows source or incompatible third-party license imports. StyleOS retains its own architecture. First scoped candidate parks only approved retry-loop sample apps on authenticated empty input; no system-wide idle sleep yet.

Owner clarification: timer-driven preemptive multitasking already exists and has hardware guest evidence. Keep that implementation; event waiting is an extension of existing scheduling, not a new preemption project. Preserve timer, fault, stop and isolation regressions.

## Measured pre-wait baseline (2026-11-27)
CI 46053092483, artifact 15225059320 (128 MiB QEMU 8.2.2 TCG, single CPU, live-volume profile). 100/100 launch/close cycles: launch-to-input median 274.40 ms, p95 286.19 ms; close-to-release median 90.28 ms, p95 92.31 ms. Actual 2372 empty input requests over 10.0884 host seconds, 235.12/s. QEMU process CPU 100.05% before/after, 100.10% with one app; these are host process measurements, not guest CPU. No total RAM or minimum-spec conclusion. Preserve raw artifact for paired comparison after wait implementation.


## Deep Drop 72

The x86-64 text broker and gesture state occupy 1200 fixed bytes including the
1024-byte payload. There is one slot, no heap or extra payload-sized kernel stack
copy. Notifications consume an existing window queue entry and wake a parked
recipient. An idle broker skips its TSC read and endpoint scans. The optional
external source is checked with one 512-byte header read on explicit F7 on legacy
provisioned media. No disk I/O is required by transfer or retirement.
The Deep Drop guest check records private page identities, exact bytes, process
retirement and disk write counters. Total guest RAM, idle CPU and Beta memory
acceptance still require separate measurements on the target configuration.
