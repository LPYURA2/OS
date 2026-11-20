#!/usr/bin/env sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT HUP INT TERM
for cc in gcc clang; do
  for app in test_documents_review stress_review; do
    "$cc" -std=c11 -Wall -Wextra -Werror -pedantic -I "$repo/include" \
      "$repo/modules/documents/review.c" "$repo/tests/$app.c" -o "$out/$app"
    "$out/$app"
  done
  "$cc" -std=c11 -Wall -Wextra -Werror -pedantic -I "$repo/include" \
      "$repo/modules/documents/model.c" "$repo/tests/test_documents_model.c" -o "$out/model"
  "$out/model"
  "$cc" -std=c11 -Wall -Wextra -Werror -pedantic -I "$repo/include" \
      "$repo/modules/documents/model.c" "$repo/modules/documents/docx.c" \
      "$repo/tests/test_documents_docx.c" -o "$out/docx"
  "$out/docx" "$out/${cc##*/}.docx"
done
python3 -m unittest discover -s "$repo/tests" -p 'test_*.py' -v
if [ "${1:-}" ]; then python3 "$repo/tools/audit_744.py" "$1"; fi
