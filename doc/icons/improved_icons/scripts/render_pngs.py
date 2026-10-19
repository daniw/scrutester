"""
Rasterizes ../svg/*.svg into ../png/<name>_256.png, <name>_96.png and
<name>_64.png via Inkscape's CLI, then rebuilds ../contact_sheet.png (3-wide
preview grid, from the 256px renders).

Requires: inkscape (>=1.0, uses --export-type/--export-filename), Pillow.
Usage: python3 render_pngs.py
"""
import os
import subprocess
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
SVG_DIR = os.path.join(ROOT, 'svg')
PNG_DIR = os.path.join(ROOT, 'png')
os.makedirs(PNG_DIR, exist_ok=True)

ORDER = ['60v_source', '10a_source', 'milliohmmeter', 'ohmmeter', 'isolation',
         'voltmeter', 'ampmeter', 'charge', 'settings', 'emi']

def render(svg_path, png_path, size):
    subprocess.run(
        ['inkscape', svg_path, '--export-type=png',
         f'--export-filename={png_path}',
         f'--export-width={size}', f'--export-height={size}'],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )

def build_contact_sheet():
    pad, label_h, cell = 16, 22, 140
    cols = 3
    rows = (len(ORDER) + cols - 1) // cols
    sheet = Image.new('RGB', (pad + cols * (cell + pad), pad + rows * (cell + label_h + pad)), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for i, name in enumerate(ORDER):
        im = Image.open(os.path.join(PNG_DIR, f'{name}_256.png')).convert('RGB').resize((cell, cell), Image.LANCZOS)
        c, r = i % cols, i // cols
        x, y = pad + c * (cell + pad), pad + r * (cell + label_h + pad)
        sheet.paste(im, (x, y))
        d.text((x, y + cell + 2), name, fill=(255, 255, 255))
    sheet.save(os.path.join(ROOT, 'contact_sheet.png'))

if __name__ == '__main__':
    for name in ORDER:
        svg_path = os.path.join(SVG_DIR, f'{name}.svg')
        render(svg_path, os.path.join(PNG_DIR, f'{name}_256.png'), 256)
        render(svg_path, os.path.join(PNG_DIR, f'{name}_64.png'), 64)
        render(svg_path, os.path.join(PNG_DIR, f'{name}_96.png'), 96)
        print('rendered', name)
    build_contact_sheet()
    print('wrote contact_sheet.png')
