"""
Generates the 10 icon SVG masters into ../svg/.

Usage: python3 gen_icons.py
(run from anywhere -- paths are relative to this script's location)
"""
import math
import os
from svghelp import pt, arc_path, tick, badge_ring, write_svg

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'svg')
os.makedirs(OUT, exist_ok=True)

FONT = "DejaVu Sans, Arial, sans-serif"

COLORS = {
    '60v_source':     '#20a5ff',
    '10a_source':     '#ffbe00',
    'milliohmmeter':  '#009594',
    'ohmmeter':       '#009d9c',
    'isolation':      '#ff0000',
    'voltmeter':      '#9ceeff',
    'ampmeter':       '#fff600',
    'charge':         '#00ff00',
    'settings':       '#dedede',
    'emi':            '#ee82ee',
}

def gauge(cx, cy, r, color, sw_arc, sw_tick, needle_len, needle_deg, needle_w, hub_r):
    body = ''
    body += f'<path d="{arc_path(cx, cy, r, 210, -30)}" fill="none" stroke="{color}" stroke-width="{sw_arc}" stroke-linecap="round"/>'
    for deg in (210, 150, 90, 30, -30):
        body += tick(cx, cy, r, r - 10, deg, stroke=color, stroke_width=sw_tick, stroke_linecap="round")
    nx, ny = pt(cx, cy, needle_len, needle_deg)
    body += f'<line x1="{cx}" y1="{cy}" x2="{nx:.2f}" y2="{ny:.2f}" stroke="{color}" stroke-width="{needle_w}" stroke-linecap="round"/>'
    body += f'<circle cx="{cx}" cy="{cy}" r="{hub_r}" fill="{color}"/>'
    return body

def meter_icon(color, label, font_size=34):
    body = badge_ring(color)
    body += gauge(128, 150, 50, color, sw_arc=6, sw_tick=5, needle_len=40, needle_deg=55, needle_w=7, hub_r=7)
    body += f'<text x="128" y="92" font-family="{FONT}" font-weight="bold" font-size="{font_size}" fill="{color}" text-anchor="middle">{label}</text>'
    return body

def source_terminal_leads(cx, top_y, bot_y, color, sw=6, r=4):
    body = f'<line x1="{cx}" y1="{top_y}" x2="{cx}" y2="{top_y-18}" stroke="{color}" stroke-width="{sw}" stroke-linecap="round"/>'
    body += f'<circle cx="{cx}" cy="{top_y-18}" r="{r}" fill="{color}"/>'
    body += f'<line x1="{cx}" y1="{bot_y}" x2="{cx}" y2="{bot_y+18}" stroke="{color}" stroke-width="{sw}" stroke-linecap="round"/>'
    body += f'<circle cx="{cx}" cy="{bot_y+18}" r="{r}" fill="{color}"/>'
    return body

def voltage_source_icon(color, label):
    cx, cy, r = 128, 112, 34
    body = badge_ring(color)
    body += f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{color}" stroke-width="6"/>'
    body += source_terminal_leads(cx, cy - r, cy + r, color)
    body += f'<text x="{cx}" y="{cy-2}" font-family="{FONT}" font-weight="bold" font-size="22" fill="{color}" text-anchor="middle">+</text>'
    body += f'<text x="{cx}" y="{cy+24}" font-family="{FONT}" font-weight="bold" font-size="26" fill="{color}" text-anchor="middle">&#8722;</text>'
    body += f'<text x="{cx}" y="200" font-family="{FONT}" font-weight="bold" font-size="30" fill="{color}" text-anchor="middle">{label}</text>'
    return body

def current_source_icon(color, label):
    cx, cy, r = 128, 112, 34
    body = badge_ring(color)
    body += f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{color}" stroke-width="6"/>'
    body += source_terminal_leads(cx, cy - r, cy + r, color)
    body += f'<line x1="{cx}" y1="{cy+22}" x2="{cx}" y2="{cy-20}" stroke="{color}" stroke-width="6" stroke-linecap="round"/>'
    body += f'<path d="M {cx-9} {cy-10} L {cx} {cy-24} L {cx+9} {cy-10} Z" fill="{color}"/>'
    body += f'<text x="{cx}" y="200" font-family="{FONT}" font-weight="bold" font-size="30" fill="{color}" text-anchor="middle">{label}</text>'
    return body

def isolation_icon(color):
    body = badge_ring(color)
    bolt = "M 143 45.5 L 103 133 L 128 133 L 108 205.5 L 165.5 115.5 L 135.5 115.5 Z"
    body += f'<path d="{bolt}" fill="{color}"/>'
    return body

def charge_icon(color):
    body = badge_ring(color)
    body += f'<rect x="72" y="94" width="104" height="68" rx="10" fill="none" stroke="{color}" stroke-width="7"/>'
    body += f'<rect x="176" y="114" width="12" height="28" rx="3" fill="{color}"/>'
    for bx in (84, 104, 124):
        body += f'<rect x="{bx}" y="104" width="12" height="48" fill="{color}" opacity="0.55"/>'
    bolt = "M 140 88 L 118 130 L 132 130 L 122 168 L 156 118 L 138 118 Z"
    body += f'<path d="{bolt}" fill="#000000" stroke="{color}" stroke-width="3" stroke-linejoin="round"/>'
    return body

def gear_icon(color):
    cx, cy = 128, 128
    r_out, r_in = 50, 34
    ring_r = (r_out + r_in) / 2
    ring_w = r_out - r_in
    body = badge_ring(color)
    body += f'<circle cx="{cx}" cy="{cy}" r="{ring_r}" fill="none" stroke="{color}" stroke-width="{ring_w}"/>'
    teeth = 8
    tooth_r = 10
    for i in range(teeth):
        deg = i * (360 / teeth)
        x, y = pt(cx, cy, r_out + 2, deg)
        body += f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{tooth_r}" fill="{color}"/>'
    body += f'<circle cx="{cx}" cy="{cy}" r="16" fill="#000000"/>'
    return body

def emi_icon(color):
    """Disturbance radiating from a switching source: a sine burst flanked by
    emission arcs, with the EMI label underneath like the source icons."""
    cx, cy = 128, 112
    body = badge_ring(color)
    # 1.5 periods of sine, 72 px wide, sampled as a polyline.
    pts = []
    for i in range(61):
        x = cx - 36 + i * 72 / 60
        y = cy - 18 * math.sin(i / 60 * 3 * math.pi)
        pts.append(f'{x:.2f},{y:.2f}')
    body += f'<polyline points="{' '.join(pts)}" fill="none" stroke="{color}" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"/>'
    for r in (48, 64):
        body += f'<path d="{arc_path(cx, cy, r, 145, 215, sweep=0)}" fill="none" stroke="{color}" stroke-width="6" stroke-linecap="round"/>'
        body += f'<path d="{arc_path(cx, cy, r, 35, -35)}" fill="none" stroke="{color}" stroke-width="6" stroke-linecap="round"/>'
    body += f'<text x="{cx}" y="200" font-family="{FONT}" font-weight="bold" font-size="30" fill="{color}" text-anchor="middle">EMI</text>'
    return body

ICONS = {
    '60v_source':    voltage_source_icon(COLORS['60v_source'], '60V'),
    '10a_source':    current_source_icon(COLORS['10a_source'], '10A'),
    'milliohmmeter': meter_icon(COLORS['milliohmmeter'], 'mΩ', font_size=28),
    'ohmmeter':      meter_icon(COLORS['ohmmeter'], 'Ω'),
    'isolation':     isolation_icon(COLORS['isolation']),
    'voltmeter':     meter_icon(COLORS['voltmeter'], 'V'),
    'ampmeter':      meter_icon(COLORS['ampmeter'], 'A'),
    'charge':        charge_icon(COLORS['charge']),
    'settings':      gear_icon(COLORS['settings']),
    'emi':           emi_icon(COLORS['emi']),
}

if __name__ == '__main__':
    for name, body in ICONS.items():
        write_svg(os.path.join(OUT, f'{name}.svg'), body)
        print('wrote', name)
