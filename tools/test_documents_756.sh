#!/bin/sh
# Stage 756 host test suite. Never claim StyleOS guest acceptance from this run.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
mkdir -p build/756 build/755
# Older Python package regressions are bound to the 755 path. Rebuild with current sources.
python3 tools/build_documents_tea.py --output build/755/Documents.tea
sh tools/test_restore.sh
SRC='modules/documents/model.c modules/documents/edit.c modules/documents/history.c modules/documents/docx.c modules/documents/import.c modules/documents/gateway.c modules/documents/ui.c modules/documents/review.c modules/documents/runtime.c'
for compiler in gcc clang; do
 "$compiler" -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
   modules/documents/model.c modules/documents/edit.c modules/documents/history.c \
   tests/test_documents_edit.c -o "build/756/edit-$compiler"
 "build/756/edit-$compiler"
 "$compiler" -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
   $SRC tests/test_documents_edit_runtime.c -o "build/756/edit-runtime-$compiler"
 "build/756/edit-runtime-$compiler"
 "$compiler" -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
   $SRC tests/test_documents_runtime.c -o "build/756/runtime-$compiler"
 "build/756/runtime-$compiler"
done
clang -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Wall -Wextra -Werror -pedantic -Iinclude \
 $SRC tests/test_documents_edit_runtime.c -o build/756/edit-runtime-sanitized
ASAN_OPTIONS=detect_leaks=1 build/756/edit-runtime-sanitized
python3 tools/build_documents_tea.py --output build/756/Documents.tea
python3 - <<'PY'
from pathlib import Path
import zipfile
with zipfile.ZipFile('build/756/Documents.tea') as z:
    Path('build/756/Documents.elf').write_bytes(z.read('payload.elf'))
PY
if nm -u build/756/Documents.elf | grep .; then
 echo 'UNRESOLVED SYMBOLS: FAIL' >&2;exit 1
fi
readelf -h build/756/Documents.elf | grep -E 'ELF64|Advanced Micro Devices|EXEC' >/dev/null
gcc -std=c11 -Wall -Wextra -Werror -Wno-pedantic -Iinclude \
 tests/test_documents_elf_host.c -o build/756/elf-host-smoke
# ELF host harness still writes to the historical build/755 path.
mkdir -p build/755
build/756/elf-host-smoke build/756/Documents.elf
# Retain the historical package test path without changing the original artifact.
cp build/756/Documents.tea build/755/Documents.tea
python3 -m unittest discover -s tests -p 'test_documents_package.py' -v
python3 - <<'PY'
import importlib.util
from pathlib import Path
p=Path('tools/build_documents_tea.py');s=importlib.util.spec_from_file_location('tea',p)
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
x=Path('build/756/Documents.tea'); assert m.verify(x)['project']['id']=='styleos.documents'
other=Path('build/756/rebuilt.tea');m.build(other)
assert x.read_bytes()==other.read_bytes(),'nonreproducible package'
print('756 .tea REPRODUCIBLE PASS')
PY
sha256sum build/756/Documents.tea build/756/Documents.elf > build/756/SHA256SUMS
printf '756 HOST TESTS PASS; STYLEOS GUEST, QEMU AND VIRTUALBOX NOT RUN\n'
