# StyleOS performance acceptance

Owner requirement: optimization is continuous, not deferred until release. Beta target VM: 4 GiB RAM / 20 GB disk. This is not a measured minimum. Preserve isolation, security checks and durability; do not trade correctness for benchmark scores.

69 initial measurements: ordinary QEMU end-to-end host monotonic launch-to-frame and close times (includes QMP and screenshot overhead), actual guest CPU trace syscall counts, input polls and disk I/O. Compare identical workload, guest resources, hypervisor version, image and host conditions. Report raw metrics, median/p95 with repeated workloads, distinguish host and physical tests; never claim speedup without paired data.

70 gate: record process/resource counts before/after normal exit, fault and repeated launch, exact owned pages and leak-free release; measure input poll rate and idle CPU, event-driven sleep/wakeup before claiming efficient native apps. Existing busy polling is a known optimization target; do not add arbitrary sleeps or weaken scheduler proofs. Check cursor damage budget and overlapping surface updates. Keep filesystem durability and large-file checks. Later performance baseline on actual 4 GiB / 20 GB VM, plus physical Intel/AMD hardware when available. No numerical target for CPU/launch latency until a reproducible baseline exists.
