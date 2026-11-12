"""
trim_font.py -- shrinks a UGUI bitmap font (Library/UGUI/Fonts/font_*.c) down
to only the characters actually referenced by the firmware, rewriting the
header/offset-table/glyph-data to match the UGUI font binary format. Written
to trim font_40x78.c (FONT_40X78/FONT_HUGE) and font_32x53.c (FONT_32X53/
FONT_BIG), the two fonts display.c only ever draws digits/punctuation/a
handful of letters with, despite each shipping the full 224-char codepage.

Font layout (see Library/UGUI/ugui.c's _UG_FontSelect()/_UG_GetCharData(),
cross-checked against the real font_40x78.c/font_32x53.c header bytes):

  Header (9 bytes): char_width, char_height, number_of_chars (u16 BE),
    offset_size (u16 BE, size in bytes of the offset table below),
    bytes_per_char (u16 BE), flags.
      flags bit 7 = is_old_font (enables a codepage-850-ish accented-char
        remap in _UG_GetCharData -- irrelevant here, none of our kept chars
        are accented, so trimming is transparent to it either way).
      flags bit 6 = width table present (neither font here has one).
      flags bits 5-0 = font_type/bpp.

  [Width table: number_of_chars bytes, only present if flags bit 6 is set.]

  Offset table (offset_size bytes): entries in *ascending* order of
  character code (required -- _UG_GetCharData's search bails out early via
  "encoding < char_start => not present" once it passes the target),
  terminated by a single 0xFF sentinel byte:
      0x00, char (u16 BE)                          -- one character
      0x01, char_start (u16 BE), char_stop (u16 BE) -- inclusive range

  Glyph data: one bytes_per_char block per character named by the offset
  table, in the same order as the offset table (bytes_per_char =
  ceil(char_width/8) * char_height for these 1bpp fonts).

This script parses the existing (full) font file, keeps only the requested
characters' glyph bitmaps, re-groups the kept character codes into the
fewest possible single/range offset entries, and rewrites the file with a
correctly rebuilt header/offset-table/data -- preserving the original
top-level `#include`/`#ifdef UGUI_USE_.../UG_FONT ...[] = { ... };/#endif`
wrapper exactly, so nothing referencing FONT_40X78/FONT_32X53 needs to
change.

Usage (regenerates both fonts in place from their *current* content, i.e.
run this only once against the full, untrimmed originals -- see the
reproducibility note below):

    python3 trim_font.py

To grow a character set later: a font file that has already been trimmed
no longer contains the dropped glyphs, so re-run this against a pre-trim
copy instead (e.g. `git show <rev-before-trim>:.../font_40x78.c > /tmp/full.c`
then point CHARSETS/trim_font() at /tmp/full.c as the source, keeping the
firmware path as the destination).
"""
import os
import re

# --- Which characters each font must keep -------------------------------
# See Core/Src/display.c for the audit: every LCD_PutStr() call using
# FONT_HUGE (=FONT_40X78) or FONT_BIG (=FONT_32X53), and every sprintf()
# format / literal string that can reach it.
CHARSETS = [
    # (filename in this dir's parent, characters to keep)
    ('font_40x78.c', ' -.0123456789'),
    ('font_32x53.c', ' -.0123456789AEORV'),
]


def read_font_c(path):
    """Split a font_*.c file into (prefix incl. opening '{', flat byte
    values, suffix incl. closing '};' and anything after)."""
    with open(path) as f:
        src = f.read()

    m = re.search(r'(UG_FONT\s+\w+\[\]\s*=?\s*\{)(.*)(\};)', src, re.S)
    if not m:
        raise ValueError('could not find a UG_FONT ...[] = { ... }; array in %s' % path)

    prefix = src[:m.start(1)] + m.group(1)
    body = m.group(2)
    suffix = m.group(3) + src[m.end(3):]

    # font_32x53.c annotates every glyph row with a trailing "// 0xNN"
    # comment -- strip //... comments first so those don't get mistaken
    # for extra data bytes by the hex-token scan below.
    body_no_comments = re.sub(r'//[^\n]*', '', body)
    values = [int(tok, 16) for tok in re.findall(r'0x[0-9A-Fa-f]+', body_no_comments)]
    return prefix, values, suffix


def parse_font(values):
    """Decode header + offset table, returning per-character glyph bitmaps
    plus the header fields needed to rebuild a trimmed version."""
    char_width = values[0]
    char_height = values[1]
    offset_size = (values[4] << 8) | values[5]
    bytes_per_char = (values[6] << 8) | values[7]
    flags = values[8]
    has_widths = bool(flags & 0x40)

    idx = 9
    if has_widths:
        # Not used by either font this script targets, but handled for
        # completeness/future fonts.
        number_of_chars = (values[2] << 8) | values[3]
        idx += number_of_chars

    offset_bytes = values[idx:idx + offset_size]
    idx += offset_size
    data_bytes = values[idx:]

    bitmaps = {}
    skip = 0
    i = 0
    while i < len(offset_bytes):
        t = offset_bytes[i]
        if t == 0xFF:
            break
        elif t == 0x00:
            code = (offset_bytes[i + 1] << 8) | offset_bytes[i + 2]
            start = skip * bytes_per_char
            bitmaps[code] = data_bytes[start:start + bytes_per_char]
            skip += 1
            i += 3
        elif t == 0x01:
            c_start = (offset_bytes[i + 1] << 8) | offset_bytes[i + 2]
            c_stop = (offset_bytes[i + 3] << 8) | offset_bytes[i + 4]
            for code in range(c_start, c_stop + 1):
                start = skip * bytes_per_char
                bitmaps[code] = data_bytes[start:start + bytes_per_char]
                skip += 1
            i += 5
        else:
            raise ValueError('unexpected offset-table entry type 0x%02X' % t)

    return {
        'char_width': char_width,
        'char_height': char_height,
        'bytes_per_char': bytes_per_char,
        'flags': flags,
        'bitmaps': bitmaps,
    }


def group_into_runs(codes):
    """Sorted char codes -> list of inclusive (start, stop) runs, merging
    consecutive codes so a run of length 1 becomes a single-char entry and
    anything longer becomes a range entry."""
    codes = sorted(codes)
    runs = []
    start = prev = codes[0]
    for c in codes[1:]:
        if c == prev + 1:
            prev = c
            continue
        runs.append((start, prev))
        start = prev = c
    runs.append((start, prev))
    return runs


def build_offset_table_and_data(font, codes):
    offset_bytes = []
    data_bytes = []
    for start, stop in group_into_runs(codes):
        if start == stop:
            offset_bytes += [0x00, (start >> 8) & 0xFF, start & 0xFF]
        else:
            offset_bytes += [0x01, (start >> 8) & 0xFF, start & 0xFF,
                              (stop >> 8) & 0xFF, stop & 0xFF]
        for code in range(start, stop + 1):
            bitmap = font['bitmaps'].get(code)
            if bitmap is None:
                raise KeyError('character %r (0x%02X) not present in source font'
                                % (chr(code), code))
            data_bytes += list(bitmap)
    offset_bytes.append(0xFF)
    return offset_bytes, data_bytes


def emit_byte_array(values, per_line=16):
    lines = []
    for i in range(0, len(values), per_line):
        row = values[i:i + per_line]
        lines.append('  ' + ','.join('0x%02X' % v for v in row) + ',')
    return '\n'.join(lines)


def trim_font(src_path, chars, dst_path=None):
    dst_path = dst_path or src_path
    prefix, values, suffix = read_font_c(src_path)
    font = parse_font(values)

    codes = sorted({ord(c) for c in chars})
    offset_bytes, data_bytes = build_offset_table_and_data(font, codes)

    number_of_chars = len(codes)
    offset_size = len(offset_bytes)
    header = [
        font['char_width'], font['char_height'],
        (number_of_chars >> 8) & 0xFF, number_of_chars & 0xFF,
        (offset_size >> 8) & 0xFF, offset_size & 0xFF,
        (font['bytes_per_char'] >> 8) & 0xFF, font['bytes_per_char'] & 0xFF,
        font['flags'],
    ]

    all_values = header + offset_bytes + data_bytes
    body = '\n' + emit_byte_array(all_values) + '\n'

    with open(dst_path, 'w') as f:
        f.write(prefix + body + suffix)

    kept = ''.join(chr(c) for c in codes)
    print('%s: %d -> %d bytes (%d chars: %r)' %
          (dst_path, len(values), len(all_values), number_of_chars, kept))


def main():
    fonts_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    for filename, chars in CHARSETS:
        trim_font(os.path.join(fonts_dir, filename), chars)


if __name__ == '__main__':
    main()
