#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for cc in gcc clang; do
 flags=''
 if [ "$cc" = clang ]; then flags='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie'; fi
 for t in monotonic calibration pit_capture; do
  $cc -std=c11 -O2 -Wall -Wextra -Werror $flags -Iinclude kernel/monotonic_855.c kernel/calibration_855.c kernel/pit_capture_855.c tests/test_${t}_855.c -o "$tmp/$t"
  "$tmp/$t"
 done
done
for src in monotonic calibration pit_capture; do
 clang --target=x86_64-w64-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -mno-red-zone -Iinclude -c "kernel/$src"_855.c -o "$tmp/$src.obj"
 objdump -t "$tmp/$src.obj" > "$tmp/$src.symbols"
 if grep -Eq '\(sec +0\)' "$tmp/$src.symbols"; then echo "Unresolved symbols: $src"; exit 1; fi
done
printf '%s\n' 'COFF x86-64 3/3 PASS: no undefined symbols'
for cc in gcc clang; do
 flags=''
 if [ "$cc" = clang ]; then flags='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie'; fi
 $cc -std=c11 -O2 -Wall -Wextra -Werror $flags -Iinclude kernel/pit_x86_gate_855.c tests/test_pit_x86_gate_855.c -o "$tmp/pitgate"
 "$tmp/pitgate"
done
for src in kernel/pit_x86_gate_855.c arch/x86_64/pit_x86_855.c; do
 clang --target=x86_64-w64-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -mno-red-zone -Iinclude -c "$src" -o "$tmp/$(basename "$src").obj"
done
objdump -d "$tmp/pit_x86_855.c.obj" > "$tmp/pit-x86-disassembly.txt"
python3 - "$tmp/pit-x86-disassembly.txt" <<'PY'
import re,sys
s=open(sys.argv[1]).read(); ops=re.findall(r'\b(in|out)\s+([^\n]+)',s)
assert ops and all(re.fullmatch(r'\$0x61,%al' if op=='in' else r'%al,\$0x(?:61|43|42)',arg.strip()) for op,arg in ops),ops
assert {arg.strip() for op,arg in ops if op=='out'}=={'%al,$0x61','%al,$0x43','%al,$0x42'}
assert 'rdtsc' in s and 'lfence' in s
print('COFF two x86 modules PASS; compiled port allowlist and TSC instruction presence PASS')
PY
