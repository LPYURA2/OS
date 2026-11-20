# StyleOS Documents — recovery source checkpoint 754

**This is host-tested recovery source, not a StyleOS installer image or a runnable Documents.tea.** Immutable system control: 744.

Contents:
- `modules/documents/model.c` — bounded UTF-8 document model and comments.
- `modules/documents/review.c` — partial offline review/tracked editing.
- `modules/documents/docx.c` — standards-shaped uncompressed DOCX export.
- `modules/documents/import.c` — matching safe ZIP_STORED DOCX text/page import.
- `modules/documents/gateway.c` — system-independent, capability-scoped file read/atomic save broker interface.
- `modules/documents/ui.c` — independent draw command list and client-area event handling for a partial UI based on 14 approved screens.
- `tools/test_restore.sh` — host tests, not OS guest tests.
- `tools/render_documents_ui.py` — optional host-only preview requiring Python Pillow.

To test:

```sh
sh tools/test_restore.sh
python3 tools/render_documents_ui.py --view review --output /tmp/documents-review-preview.png
```

Details and unimplemented functionality: `docs/RECOVERY_754_RU.md`. Continuation point: `docs/CONTINUE.md`.
