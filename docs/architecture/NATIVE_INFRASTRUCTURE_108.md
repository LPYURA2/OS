# Stage 108 — bounded native infrastructure and SDK acceptance candidate

Base: c2aadb7b5476616d50433498c605c0d009d97bbd (107). 106 CI 46140590452: 41/41 SUCCESS.
107 CI 46140846973: main installation/cold launch guest SUCCESS; full run still in progress at preparation.
Do not accept 108 until both 107 and 108 are fully successful.

## Scope
Re-run the 39 existing job definitions (42 job instances because fault-acceptance
has four matrix probes) on the exact new SHA, plus a fail-closed aggregate gate.
The gate rejects skipped, failed, cancelled, absent and unexpected dependencies.
It verifies downloaded same-run source archive and ordinary boot ISO checksums,
and compares source commit/tree identities to the checkout. The receipt is
published only after the dependency and artifact checks pass.
The 108-only ordinary ISO artifact name is styleos-native-apps-iso.

Coverage: independent CPL3 execution, timer preemption, faults and forced
retirement, SDK ABI/wire and compiled guest C client, graphics/input/text,
file access, writes and cold recovery, bounded Deep Drop including minimized
destination and recovery, approved .tea registry and installation with cold
launch after original removal, ordinary shell and StyleFS live regression.
No runtime change; do not claim general application compatibility.

## Limits and next step
Existing tests provide bounded resource accounting, not a new idle RAM/CPU
benchmark or a guarantee of <1 GiB RAM. Target VM 4 GiB RAM / 20 GB disk;
VirtualBox/physical acceptance for this SHA remains unverified. No general
publisher trust store, automatic Apps provisioning, installed-app catalog UI,
update/uninstall catalog, or final installer design. Deep Drop generalized
public SDK for arbitrary future apps remains open. Preserve all prior refs,
Astra work and main. After full acceptance, ask owner for actual app mockups
before product UI changes; do not claim production or release readiness.
