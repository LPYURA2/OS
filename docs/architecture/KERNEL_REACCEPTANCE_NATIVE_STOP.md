# Kernel reacceptance after native forced stop and ownership checks

Base 11425c9506a22a761e1eb72f935885aa04234ebc. Previous exact guest forced-stop SHA 6430b530310a12bc71e2871960f7f36340c4ea92: Actions 45837572666, 4/4 SUCCESS; guest artifact 15125268708. Ownership candidate Actions 45837874543 was not fully complete when this reacceptance branch was prepared. This is a parallel candidate regression, NOT a release or accepted kernel.

Branch feature/styleos-kernel-reacceptance. No kernel, application, verifier, test expectations or build commands changed. Four workflows are enabled on this branch, including the 59 historical conditional checks previously enabled for final acceptance/fault audit. Exact SHA must be verified for every run. Old branch status publication must remain skipped. Do not treat a queued/running/cancelled/skipped mandatory job as PASS.

| Gate | Required evidence in this revision | State |
|---|---|---|
| KFA-01 | Exact archive and ordinary no-catalog independent EFI/IMG/ISO reproduction | OPEN |
| KFA-02 | Baseline handoff/EBS and ordinary root/descriptor snapshot | OPEN |
| KFA-03 | Historical protection, cache, guard, mapping audits and native fault trace | OPEN |
| KFA-04 | Arena refusals, physical lease reuse and scrubbing | OPEN |
| KFA-05 | Real LAPIC 32-context timer-load and infinite-loop IRQ tests | OPEN |
| KFA-06 | Endurance, FP and actual scheduling/stop/peer survival | OPEN |
| KFA-07 | Actual CPL3/fault/INT80 tests, stale/unauthorized stop, ownership and reuse | OPEN |
| KFA-08 | Historical IST and real malformed RSP/DE/NX/DB/GP/UD tests | OPEN |
| KFA-09 | ACPI/PCI/DMA and Intel/AMD diagnostic evidence within existing admitted scope | OPEN |
| KFA-10 | No-catalog ordinary ISO boot, extended Desktop, save/reopen and foreign disk refusal | OPEN |
| KFA-11 | Repeated mixed workload and storage boots; stop/ownership and peer survival | OPEN |
| KFA-12 | All mandatory historical + baseline x86/ARM checks and independent raw evidence review | OPEN |

Remaining limits irrespective of CI: ordinary kernel trusted native launch/stop API is not wired to end-user .tea installation or task-manager action; IPC ownership fixture uses privileged APIs, not user-mode graphics. One vCPU Q35/TCG scope, no claim of general RAM allocator, broad hardware/SMP/VirtualBox support or full production fault coverage. Performance/system requirements and independent audit remain separate deliverables. Do not label Style Kernel 1.0 or merge main until all mandatory gates and owner approval. See KERNEL_FINAL_ACCEPTANCE.md and FOUNDATION_COMPLETION_GATE.md. 
