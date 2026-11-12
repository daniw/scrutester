"""Small SVG-authoring helpers shared by gen_icons.py."""
import math

def pt(cx, cy, r, deg):
    a = math.radians(deg)
    return (cx + r * math.cos(a), cy - r * math.sin(a))  # y-up math -> svg y-down

def arc_path(cx, cy, r, deg_start, deg_end, sweep=1):
    x1, y1 = pt(cx, cy, r, deg_start)
    x2, y2 = pt(cx, cy, r, deg_end)
    large = 1 if abs(deg_end - deg_start) % 360 > 180 else 0
    return f"M {x1:.2f} {y1:.2f} A {r:.2f} {r:.2f} 0 {large} {sweep} {x2:.2f} {y2:.2f}"

def tick(cx, cy, r1, r2, deg, **attrs):
    x1, y1 = pt(cx, cy, r1, deg)
    x2, y2 = pt(cx, cy, r2, deg)
    a = ' '.join(f'{k.replace("_","-")}="{v}"' for k, v in attrs.items())
    return f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" {a}/>'

SVG_HEAD = '''<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 256 256">
<rect x="0" y="0" width="256" height="256" fill="#000000"/>
'''
SVG_TAIL = '</svg>'

def badge_ring(color, r=98, sw=10, cx=128, cy=128):
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{color}" stroke-width="{sw}"/>'

def write_svg(path, body, size=256):
    with open(path, 'w') as f:
        f.write(SVG_HEAD.format(size=size))
        f.write(body)
        f.write(SVG_TAIL)
