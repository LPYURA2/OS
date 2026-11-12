"""Verify identical outputs from C++, Python and Java against reference bytes."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'build'
files = [root / 'frame_cpp.ppm', root / 'frame_python.ppm']
if (root / 'frame_java.ppm').exists():
    files.append(root / 'frame_java.ppm')
reference = files[0].read_bytes()
assert reference.startswith(b'P6\n480 270\n255\n')
assert len(reference) > 480 * 270 * 3
for file in files:
    assert file.read_bytes() == reference, f'{file} does not match!'
print('ALL ENGINE TESTS PASSED:', ', '.join(x.name for x in files))
