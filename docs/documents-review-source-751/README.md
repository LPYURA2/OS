# StyleOS — Documents review source checkpoint (751-source)

This directory contains the **independent C11 grammar-hints and tracked-changes
engine**, host tests and integration notes for native `Documents.tea`.
It is **not** a bootable StyleOS build, a new ISO, or an installed `.tea` app.

The existing `Documents.tea` sources at the previously claimed 749 checkpoint
are not available in the accessible GitHub repository. The 744 delivery ZIP
contains ISO/VDI and build notes, not its source tree. Therefore this source
checkpoint must be **cherry-picked/ported onto the actual Documents source**
before the functionality can appear in the UI. Do not downgrade a 744–749
working image by building the older GitHub checkout as if it were current.

## Included APIs

- `srev_grammar_scan`: four deliberately limited UTF-8, RU/EN heuristics.
- `srev_insert` / `srev_delete`: pending insertion and deletion fragments.
- `srev_collect_changes`: marked spans with visible/original UTF-8 offsets,
  suitable for editor highlighting and selecting revisions.
- `srev_resolve` / `srev_resolve_all`: accept/reject one or all tracked edits.
- `srev_pack` / `srev_unpack`: private test-only binary state; NOT DOCX,
  `.tea`, or StyleOS user file format.

## Host test commands

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -I include \
  modules/documents/review.c tests/test_documents_review.c -o /tmp/styleos-review-unit
/tmp/styleos-review-unit
cc -std=c11 -Wall -Wextra -Werror -pedantic -I include \
  modules/documents/review.c tests/stress_review.c -o /tmp/styleos-review-stress
/tmp/styleos-review-stress
```

Also checked separately with GCC and Clang under AddressSanitizer and
UndefinedBehaviorSanitizer. These tests validate only the isolated engine.

See `docs/INTEGRATION_RU.md` for how to connect native Documents UI, reuse
existing language state, preserve comments and page geometry, and implement
round-trip DOCX tracked changes before claiming the application finished.
