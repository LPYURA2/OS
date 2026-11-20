#!/bin/sh
# Self-contained host tests. This is NOT a QEMU or StyleOS guest test.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
mkdir -p build/755
sh tools/test_restore.sh
for cc in gcc clang; do
 "$cc" -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  modules/documents/model.c modules/documents/docx.c modules/documents/import.c \
  modules/documents/gateway.c modules/documents/ui.c modules/documents/review.c \
  modules/documents/runtime.c tests/test_documents_runtime.c \
  -o "build/755/runtime-$cc"
 "build/755/runtime-$cc"
done
clang -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Wall -Wextra -Werror -pedantic -Iinclude \
 modules/documents/model.c modules/documents/docx.c modules/documents/import.c \
 modules/documents/gateway.c modules/documents/ui.c modules/documents/review.c \
 modules/documents/runtime.c tests/test_documents_runtime.c \
 -o build/755/runtime-sanitized
build/755/runtime-sanitized
python3 tools/build_documents_tea.py --output build/755/Documents.tea
python3 - <<'PY'
from pathlib import Path
import zipfile
with zipfile.ZipFile('build/755/Documents.tea') as z:
    Path('build/755/Documents.elf').write_bytes(z.read('payload.elf'))
PY
if nm -u build/755/Documents.elf | grep .; then
 echo 'UNRESOLVED: FAIL' >&2;exit 1
fi
readelf -h build/755/Documents.elf | grep -E 'ELF64|Advanced Micro Devices|EXEC' >/dev/null
cc -std=c11 -Wall -Wextra -Werror -Wno-pedantic -Iinclude \
 tests/test_documents_elf_host.c -o build/755/elf-host-smoke
build/755/elf-host-smoke build/755/Documents.elf
python3 -m unittest discover -s tests -p 'test_documents_package.py' -v
sha256sum build/755/Documents.tea > build/755/Documents.tea.sha256
sha256sum /mnt/data/styleos-recovery-744/StyleOS-Install-x64.iso > build/755/control-744.sha256
printf 'ALL 755 HOST TESTS PASS; STYLEOS GUEST/QEMU NOT RUN\n'
