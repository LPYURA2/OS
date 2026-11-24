#!/bin/sh
# Compile the actual linked kernel modules, not copies from a test fixture.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
sources='kernel/rtc_decode.c kernel/rtc_acquire.c kernel/rtc_cmos_854.c kernel/rtc_owner_854.c kernel/rtc_x86_gate_854.c'
for cc in gcc clang; do
    flags='-std=c11 -O2 -Wall -Wextra -Werror'
    if [ "$cc" = clang ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie"; fi
    for test in test_rtc_decode test_rtc_acquire test_rtc_cmos_854 test_rtc_owner_854 test_rtc_x86_gate_854; do
        echo "$cc $test"
        $cc $flags -Iinclude $sources "tests/$test.c" -o "$tmp/$test"
        "$tmp/$test"
    done
done
for src in $sources arch/x86_64/rtc_x86_854.c; do
    clang --target=x86_64-w64-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -mno-red-zone -Iinclude -c "$src" -o "$tmp/$(basename "$src").obj"
done
objdump -d "$tmp/rtc_x86_854.c.obj" > "$tmp/disasm"
python3 - "$tmp/disasm" <<'PY'
import re,sys
s=open(sys.argv[1]).read()
ops=re.findall(r'\b(?:in|out)[bwl]?\s+[^\n]+',s)
assert len(ops)==2 and any(re.match(r'out\s+%al,\$0x70',x) for x in ops) and any(re.match(r'in\s+\$0x71,%al',x) for x in ops),ops
print('COFF: six modules PASS; port instruction allowlist PASS')
PY
