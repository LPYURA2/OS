# StyleFS 47 — scoped selected-file save candidate

Base 7b7008917d20d317268a34a8e94ad9b2fc3a3836; 46 CI 45975481777 9/9 SUCCESS. Extend existing file broker with separate explicit read+save grant, exact selected revision, authenticated live process PID/handle/root, bounded 32 KiB trusted kernel input and empty replacement. Reuse wv64_replace and retained extents, no callback from untrusted caller. Committed mutation invalidates old tokens; readonly and foreign identity deny. Host process-broker tests include old read-only grant denial, invalid spans, successful save, stale tokens, cold reopen, empty replacement. CI pending; no actual CPL3 save syscall, no bulk save API or normal desktop integration yet. Next: safe user copy-in and actual guest save, then normal mount, Deep Drop and independent .tea apps. Preserve 4GB/20GB VM target, legal gate, approved UI, previous refs and main.

## New chat
Continue StyleOS LPYURA2/OS from feature/styleos-stylefs-scoped-save-47. Read AGENTS.md, docs/PROJECT.md, docs/NEXT_CHAT_STYLEFS_47.md, owner requirements and roadmap. Verify exact CI before proceeding. New stage new branch, report source/verified ISO/next point. No main changes.
