#!/bin/sh
# Preinstall host tests only. No claim of native StyleOS 744 installation.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
mkdir -p build/757
printf '%s\n' '================ 756 application regression =================='
sh tools/test_documents_756.sh
printf '%s\n' '================ 757 provisioning regression ================'
python3 -m unittest -v tests.test_tea_package tests.test_system_apps tests.test_preinstall_bundle
python3 tools/build_preinstall_bundle.py build --tea build/756/Documents.tea --output build/757/StyleOS-Documents-System-Preinstall-DEV.zip
python3 tools/build_preinstall_bundle.py verify build/757/StyleOS-Documents-System-Preinstall-DEV.zip
python3 - <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path("tools").resolve()))
import build_preinstall_bundle as b
x=Path('build/757/StyleOS-Documents-System-Preinstall-DEV.zip')
y=Path('build/757/preinstall-repeated.zip')
b.make(Path('build/756/Documents.tea'), y)
assert x.read_bytes()==y.read_bytes(),'bundle non-reproducible'
print('PREINSTALL BUILD REPRODUCIBILITY PASS')
PY
rm -rf build/757/sample-system-volume
mkdir -p build/757/sample-system-volume
python3 tools/build_preinstall_bundle.py stage-bundle --root build/757/sample-system-volume --bundle build/757/StyleOS-Documents-System-Preinstall-DEV.zip --profile development
python3 tools/system_apps.py show --root build/757/sample-system-volume > build/757/staged-catalog.txt
python3 tools/system_apps.py verify --root build/757/sample-system-volume
if python3 tools/build_preinstall_bundle.py stage-bundle --root build/757/sample-system-volume --bundle build/757/StyleOS-Documents-System-Preinstall-DEV.zip --profile production > build/757/production-refusal.log 2>&1; then
 echo 'PRODUCTION GATE TEST FAILED' >&2; exit 1
fi
printf '%s\n' 'PRODUCTION POLICY REFUSAL PASS'
sha256sum build/756/Documents.tea build/757/StyleOS-Documents-System-Preinstall-DEV.zip > build/757/SHA256SUMS
if [ -f /mnt/data/styleos-work/control-744/StyleOS-Install-x64.iso ]; then
 echo '011dd3560e8d232b00201623443d9a09b0c67e03227b66504312a31f75e285ae  /mnt/data/styleos-work/control-744/StyleOS-Install-x64.iso' | sha256sum -c -
else
 echo 'CONTROL 744 NOT AVAILABLE FOR THIS HOST TEST (SKIPPED)'
fi
printf '%s\n' '757 HOST PREINSTALL CONTRACT PASS; 744 GUEST, QEMU, VIRTUALBOX NOT RUN'
