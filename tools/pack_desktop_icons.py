#!/usr/bin/env python3
"""Pack the original transparent PNG icons into the built-in desktop raster table."""
import argparse
from pathlib import Path

from PIL import Image


NAMES = ('settings', 'explorer', 'text-editor', 'notes', 'processes', 'documents')


def pack(source):
    yield '/* Generated from assets/desktop/icons by tools/pack_desktop_icons.py. */'
    yield 'enum { DESK_ICON_PIXELS = 32 };'
    yield 'static const uint16_t app_icon_pixels[6][DESK_ICON_PIXELS * DESK_ICON_PIXELS] = {'
    for name in NAMES:
        with Image.open(source / (name + '.png')) as original:
            rgba = original.convert('RGBA')
        bounds = rgba.getchannel('A').getbbox()
        if bounds is None:
            raise ValueError(f'empty icon: {name}')
        icon = rgba.crop(bounds)
        icon.thumbnail((30, 30), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA', (32, 32))
        canvas.paste(icon, ((32 - icon.width) // 2, (32 - icon.height) // 2))
        def pixel(r, g, b, a):
            nibble = lambda value: (value + 8) // 17
            return (nibble(r) << 12 | nibble(g) << 8 | nibble(b) << 4 | nibble(a)) if nibble(a) else 0
        data = canvas.get_flattened_data() if hasattr(canvas, 'get_flattened_data') else canvas.getdata()
        values = [pixel(*p) for p in data]
        yield f'    {{ /* {name} */'
        for j in range(0, len(values), 16):
            yield '        ' + ', '.join(f'0x{value:04x}' for value in values[j:j + 16]) + ','
        yield '    },'
    yield '};'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    args.output.write_text('\n'.join(pack(args.source)) + '\n', encoding='ascii')


if __name__ == '__main__':
    main()
