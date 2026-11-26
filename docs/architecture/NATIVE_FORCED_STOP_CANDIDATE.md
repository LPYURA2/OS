# Native forced stop candidate
Base a77437baec01ab68ddb480228ace70977913d05b: exact QEMU 8.2.2 hang coexistence CI 45835660124 SUCCESS.

Trusted kernel-only s865_native_stop(pid,generation,authorized) checks quiescence, safe root, current identity and live arena lease. No syscall or automatic CPU-use policy. READY only, reserve two serial updates, no fabricated dispatch or trap. Existing PR_EXITED is used for forced supervisor exit; integer state and windows cleared. Reuse identity-based terminal cleanup, reconcile session/window channels, retire suspended FP, root tables, lease and registry. Unexpected internal invariant failure remains fail-closed.

New CI host model and existing QEMU hang regression do not prove the forced stop path in the guest. Next mandatory gate: real preempted loop, trusted stop, unauthorized/stale refusals, no old-generation dispatch, reuse resources with fresh PID/generation, live peer and Desktop continue; bind UART to CPU trace. Then complete regression and independent audit. No physical or VirtualBox claim.
