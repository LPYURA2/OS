# Stage 109 — installed application launch without restarting

Base 23582d8a39ba5a0c0722c00195c675d65efa51e9 (108 candidate). 107: 42/42 SUCCESS, CI 46140846973.
108 CI 46141175090 still pending at preparation. Do not accept 109 until both
108 and 109 have fully succeeded.

Owner requirement: installing a normal native application must not require a
system restart. Explicit installation and launching are separate operations.
The previous 107 guest test verified installation and a cold launch but did
not launch the installed copy during the installation boot.

109 extends the existing guest acceptance: cancel, explicit install, remove
original source, navigate to Apps and launch installed ID8 in the same QEMU
session, check actual process parking, Processes stop and complete resource
retirement. Require one Desktop READY receipt and ordered install/delete/launch
receipts. Then retain the independent cold launch and zero cold volume writes.
All existing 108 dependencies and aggregate gate are retained for 109. Runtime,
kernel, and product UI are unchanged. This tests an existing path; do not claim
success until the exact CI result is known. Final installer/UI, catalog,
generalized Deep Drop, publisher policy and measured RAM remain separate.
