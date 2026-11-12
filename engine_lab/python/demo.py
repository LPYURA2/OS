"""Python ctypes host calls the exact same C/C++ engine shared library."""
import ctypes
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if sys.platform == "win32":
    LIB = ROOT / 'build' / 'matryoshka_engine.dll'
elif sys.platform == 'darwin':
    LIB = ROOT / 'build' / 'libmatryoshka_engine.dylib'
else:
    LIB = ROOT / 'build' / 'libmatryoshka_engine.so'
engine = ctypes.CDLL(str(LIB))
engine.mw_create.argtypes = [ctypes.c_int, ctypes.c_int]
engine.mw_create.restype = ctypes.c_void_p
engine.mw_scene.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
engine.mw_save_ppm.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
engine.mw_save_ppm.restype = ctypes.c_int
engine.mw_checksum.argtypes = [ctypes.c_void_p]
engine.mw_checksum.restype = ctypes.c_uint32
engine.mw_destroy.argtypes = [ctypes.c_void_p]
e = engine.mw_create(480, 270)
if not e:
    raise RuntimeError('Cannot create framebuffer')
try:
    engine.mw_scene(e, 47)
    outfile = ROOT / 'build' / 'frame_python.ppm'
    if not engine.mw_save_ppm(e, os.fsencode(outfile)):
        raise OSError('Saving image failed')
    print(f'Python host: saved {outfile} checksum={engine.mw_checksum(e):08x}')
finally:
    engine.mw_destroy(e)
