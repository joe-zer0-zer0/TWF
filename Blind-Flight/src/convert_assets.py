#!/usr/bin/env python3
"""
Blind Flight — Session 8 Asset Converter
=========================================
1. Converts PNG images to RGB565 C header arrays for TFT_eSPI pushImage()
2. Extracts the dotted flight path from the SVG as a waypoint lookup table

PNGs are black line art on white backgrounds (JPEG-encoded with artifacts).
We invert and apply a noise floor so the art becomes clean light-on-black.
Black pixels (0x0000) serve as the transparent color key for sprites.
"""

import os, re, math
from PIL import Image

UPLOAD_DIR = "/mnt/user-data/uploads"
OUTPUT_DIR = "/home/claude/include"
SCREEN_W = 240
SCREEN_H = 280
NUM_WAYPOINTS = 120

# Noise floor: after inversion, values below this are forced to 0 (transparent).
# Cleans up JPEG compression artifacts in the "white" background areas.
NOISE_FLOOR = 30

IMAGES = {
    "TWF_Full_Logo_240px_no_bg.png": "logo_full",
    "Text_240x63px.png":             "logo_text",
    "plane_20x12px.png":             "plane_20",
    "plane_25x15px.png":             "plane_25",
    "plane_34x20px.png":             "plane_34",
}

def rgb_to_565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert_png_to_header(png_path, var_prefix):
    img = Image.open(png_path).convert("L")
    w, h = img.size
    pixels = list(img.getdata())

    # Invert + noise floor
    inverted = []
    for p in pixels:
        v = 255 - p
        if v < NOISE_FLOOR:
            v = 0
        inverted.append(v)

    rgb565_data = [rgb_to_565(g, g, g) for g in inverted]

    # Count transparent vs opaque for stats
    transparent = sum(1 for v in rgb565_data if v == 0x0000)

    lines = []
    lines.append(f"#pragma once")
    lines.append(f"// Auto-generated — source: {os.path.basename(png_path)}")
    lines.append(f"// Inverted grayscale → RGB565, 0x0000 = transparent")
    lines.append(f"// Noise floor: {NOISE_FLOOR}/255")
    lines.append(f"")
    lines.append(f"#include <Arduino.h>")
    lines.append(f"")
    lines.append(f"#define {var_prefix.upper()}_W {w}")
    lines.append(f"#define {var_prefix.upper()}_H {h}")
    lines.append(f"")
    lines.append(f"const uint16_t {var_prefix}_data[] PROGMEM = {{")

    for i in range(0, len(rgb565_data), 8):
        chunk = rgb565_data[i:i+8]
        hex_vals = ", ".join(f"0x{v:04X}" for v in chunk)
        comma = "," if i + 8 < len(rgb565_data) else ""
        lines.append(f"    {hex_vals}{comma}")

    lines.append(f"}};")
    return "\n".join(lines), w, h, len(rgb565_data), transparent

# ============================================================
# SVG Flight Path
# ============================================================

def cubic_bezier(p0, p1, p2, p3, t):
    u = 1 - t
    return (u**3*p0[0] + 3*u**2*t*p1[0] + 3*u*t**2*p2[0] + t**3*p3[0],
            u**3*p0[1] + 3*u**2*t*p1[1] + 3*u*t**2*p2[1] + t**3*p3[1])

def extract_flight_path():
    d = ("m 172.03856,211.96019 c -3.73514,0.534 -7.47027,1.06801 -17.31987,3.11024 "
         "-9.8496,2.04223 -25.81332,5.59261 -38.64201,11.35223 "
         "-12.8287,5.75961 -22.521123,13.72782 -29.293211,19.54711 "
         "-6.772088,5.81929 -10.622981,9.48892 -13.291142,11.47281 "
         "-2.66816,1.9839 -4.15327,2.28182 -5.5963,2.32838 "
         "-1.443029,0.0466 -2.843824,-0.15825 -3.623903,-1.02996 "
         "-0.78008,-0.87172 -0.939355,-2.41025 0.153052,-4.52295 "
         "1.092408,-2.1127 3.436359,-4.79932 5.567818,-5.78483 "
         "2.131459,-0.98551 4.05015,-0.26978 3.782656,1.01808 "
         "-0.267494,1.28786 -2.721137,3.14768 -5.033039,4.32029 "
         "-2.311903,1.17261 -4.481767,1.65785 -6.722229,2.86991 "
         "-2.240463,1.21206 -4.551289,3.15082 -6.714792,4.9045 "
         "-2.163502,1.75368 -4.179626,3.32223 -15.375441,5.63757 "
         "-11.195815,2.31535 -31.5713212,5.3775 -52.207968,5.93174 "
         "-20.636647,0.55424 -41.534405,-1.39942 -60.309979,-0.72262 "
         "-18.775574,0.6768 -35.428932,3.98406 -52.082292,7.29133")

    mat = [0.47549312, 0, 0, 0.47549312, 77.428287, 75.864152]
    parent_dx, parent_dy = -18.091197, -164.51285
    svg_w, svg_h = 170.348, 44.645

    tokens = re.findall(r'[mcMClLhHvVsSqQtTaAzZ]|[-+]?[0-9]*\.?[0-9]+', d)
    commands, i = [], 0
    while i < len(tokens):
        if tokens[i].isalpha():
            cmd = tokens[i]; i += 1; coords = []
            while i < len(tokens) and not tokens[i].isalpha():
                coords.append(float(tokens[i])); i += 1
            commands.append((cmd, coords))
        else: i += 1

    points_raw, cur = [], (0,0)
    for cmd, coords in commands:
        if cmd == 'm':
            cur = (cur[0]+coords[0], cur[1]+coords[1])
            points_raw.append(cur)
        elif cmd == 'c':
            for j in range(0, len(coords), 6):
                p0 = cur
                p1 = (cur[0]+coords[j], cur[1]+coords[j+1])
                p2 = (cur[0]+coords[j+2], cur[1]+coords[j+3])
                p3 = (cur[0]+coords[j+4], cur[1]+coords[j+5])
                for k in range(1, 9):
                    points_raw.append(cubic_bezier(p0, p1, p2, p3, k/8))
                cur = p3

    # Transform chain: path matrix → parent translate → viewBox scale
    def xform(x, y):
        tx = mat[0]*x + mat[2]*y + mat[4] + parent_dx
        ty = mat[1]*x + mat[3]*y + mat[5] + parent_dy
        return tx, ty

    svg_pts = [xform(x, y) for x, y in points_raw]

    scale = SCREEN_W / svg_w
    display_h = svg_h * scale
    offset_y = (SCREEN_H - display_h) / 2

    scaled = [(int(round(x * scale)), int(round(y * scale + offset_y)))
              for x, y in svg_pts]

    # Reverse: SVG path is right-to-left, animation needs left-to-right
    scaled.reverse()

    # Subsample
    if len(scaled) > NUM_WAYPOINTS:
        step = len(scaled) / NUM_WAYPOINTS
        scaled = [scaled[int(i * step)] for i in range(NUM_WAYPOINTS)]

    return scaled

def generate_waypoint_header(waypoints):
    lines = ["#pragma once",
             "// Auto-generated — flight path from SVG Bézier curve",
             "// Scaled to 240×280 display, left-to-right animation order",
             "", "#include <Arduino.h>", "",
             f"#define FLIGHT_PATH_LEN {len(waypoints)}", "",
             "const int16_t flight_path[][2] PROGMEM = {"]
    for i, (x, y) in enumerate(waypoints):
        x = max(0, min(SCREEN_W-1, x))
        y = max(0, min(SCREEN_H-1, y))
        comma = "," if i < len(waypoints)-1 else ""
        lines.append(f"    {{{x:4d}, {y:4d}}}{comma}")
    lines.append("};")
    return "\n".join(lines)

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print("=" * 60)
    print("Blind Flight — Asset Converter")
    print("=" * 60)

    print("\n--- PNG → RGB565 Headers ---\n")
    for filename, prefix in IMAGES.items():
        png_path = os.path.join(UPLOAD_DIR, filename)
        if not os.path.exists(png_path): continue
        header, w, h, n, transp = convert_png_to_header(png_path, prefix)
        with open(os.path.join(OUTPUT_DIR, f"img_{prefix}.h"), 'w') as f:
            f.write(header)
        pct = transp/n*100
        print(f"  {filename}")
        print(f"    → img_{prefix}.h  ({w}×{h}, {n*2:,}B, {pct:.0f}% transparent)")

    print("\n--- SVG Flight Path → Waypoints ---\n")
    waypoints = extract_flight_path()
    with open(os.path.join(OUTPUT_DIR, "flight_path.h"), 'w') as f:
        f.write(generate_waypoint_header(waypoints))
    xs = [w[0] for w in waypoints]
    ys = [w[1] for w in waypoints]
    print(f"  {len(waypoints)} waypoints, X: {min(xs)}–{max(xs)}, Y: {min(ys)}–{max(ys)}")

    # ASCII visualization of the flight path
    print("\n--- Path Preview (40×12 ASCII) ---\n")
    grid_w, grid_h = 40, 12
    grid = [['.' for _ in range(grid_w)] for _ in range(grid_h)]
    for x, y in waypoints:
        gx = int(x / SCREEN_W * (grid_w-1))
        gy = int(y / SCREEN_H * (grid_h-1))
        gx = max(0, min(grid_w-1, gx))
        gy = max(0, min(grid_h-1, gy))
        grid[gy][gx] = '*'
    for row in grid:
        print("  " + "".join(row))

    print(f"\n--- Flash: {sum(Image.open(os.path.join(UPLOAD_DIR,f)).size[0]*Image.open(os.path.join(UPLOAD_DIR,f)).size[1]*2 for f in IMAGES if os.path.exists(os.path.join(UPLOAD_DIR,f)))+len(waypoints)*4:,} bytes ({63.5:.1f} KB) ---")

if __name__ == "__main__":
    main()