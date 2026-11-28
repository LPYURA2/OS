# Stage 77 — native SDK candidate

Base: accepted 76 code 2436b7bf11e497bcfe04eaef04e641a654a476b7, CI 46128413040 15/15 PASS. See docs/test-results/NATIVE_APPS_76.md.
New branch feature/styleos-native-apps-77 introduces a user-only SDK header,
versioned input decoder, bounded window wrappers, x86-64 trap transport and
host ABI parity tests. No production kernel changes. CI pending. No generic
runtime negotiation or file ABI yet. Buffers must be in private DATA pages;
do not pass stack addresses. Compile-time version is not discovery.

Next: validate CI, build a real freestanding independent executable using
these wrappers, run it in CPL3 with actual IPC and compare raw trace. Establish
a reusable linker layout and bounded package/staging contract, then graphics
(text/fonts, Unicode), scoped file API and actual standalone Editor/Notes.
Do not prematurely claim SDK complete or migrate the shell UI before tests.
Preserve Deep Drop 72–74, 75/76 guest regressions, all earlier refs, main,
approved designs, 4 GiB/20 GB Beta and idle <1 GiB target (not measured).

Astra: limit exhausted again at b6502099077a8ec0b85b4753f46386ae1395f0f8.
All published work is an ancestor of the accepted 76 checkpoint. Inspect
your unpublished changes and reconcile without resets or force pushes.
Continue from current HEAD. No concurrent writers. Keep exact CI/SHA/ISO
and concise handoff. See docs/NEXT_CHAT_NATIVE_APPS_75.md and _76.md.
