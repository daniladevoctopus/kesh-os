#!/usr/bin/env python3
"""
Generate animated Material You 'Hello' language speech bubbles.
Languages: English (Hello), Russian (Привет), French (Bonjour), German (Hallo), Spanish (¡Hola), Italian (Ciao).
"""

import math
import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'base', 'applications', 'oobe', 'res')
os.makedirs(OUT_DIR, exist_ok=True)

WIDTH, HEIGHT = 240, 240

def create_circular_background(draw, center, radius, color_start, color_end):
    cx, cy = center
    for r in range(radius, 0, -1):
        t = r / radius
        r_c = int(color_start[0] * t + color_end[0] * (1 - t))
        g_c = int(color_start[1] * t + color_end[1] * (1 - t))
        b_c = int(color_start[2] * t + color_end[2] * (1 - t))
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(r_c, g_c, b_c, 255))

def draw_pill_bubble(draw, x, y, w, h, bg_color, border_color, text, text_color):
    draw.rounded_rectangle([x, y, x + w, y + h], radius=h // 2, fill=bg_color, outline=border_color, width=2)
    # Draw simple centered text using default font or basic bitmap font
    # Estimate text length
    tw = len(text) * 7
    tx = x + (w - tw) // 2
    ty = y + (h - 12) // 2
    draw.text((tx, ty), text, fill=text_color)

def draw_hello_frame(frame_idx, total_frames=4):
    im = Image.new("RGBA", (WIDTH, HEIGHT), (255, 252, 250, 255))
    draw = ImageDraw.Draw(im)

    # Ambient soft glow circle
    create_circular_background(draw, (120, 120), 108, (244, 236, 228), (255, 252, 250))

    # Phase offset for bobbing
    phase = (frame_idx / total_frames) * math.pi * 2

    # Bubble definitions: (text, w, h, base_x, base_y, phase_shift, amp, bg, border, text_color)
    bubbles = [
        # Orbiting bubbles
        ("Bonjour!", 78, 28, 25, 45, 0.0, 4.0, (255, 235, 230, 255), (240, 170, 150, 255), (160, 60, 40, 255)),
        ("Привет!", 72, 28, 145, 40, 1.2, 5.0, (255, 243, 225, 255), (230, 180, 120, 255), (150, 90, 30, 255)),
        ("¡Hola!", 62, 26, 20, 155, 2.5, 4.0, (235, 246, 238, 255), (150, 205, 170, 255), (40, 120, 70, 255)),
        ("Hallo!", 62, 26, 155, 158, 3.8, 5.0, (238, 243, 255, 255), (160, 185, 235, 255), (50, 80, 160, 255)),
        ("Ciao!", 56, 24, 92, 192, 4.7, 3.0, (252, 238, 250, 255), (220, 170, 215, 255), (130, 50, 120, 255)),
    ]

    for text, w, h, bx, by, p_shift, amp, bg, border, tc in bubbles:
        dy = int(math.sin(phase + p_shift) * amp)
        draw_pill_bubble(draw, bx, by + dy, w, h, bg, border, text, tc)

    # Big main central "Hello" capsule bubble
    c_dy = int(math.cos(phase) * 3.0)
    cx, cy, cw, ch = 55, 100 + c_dy, 130, 46
    # Subtle drop shadow
    draw.rounded_rectangle([cx + 3, cy + 4, cx + cw + 3, cy + ch + 4], radius=ch // 2, fill=(210, 195, 185, 140))
    # Main chocolate capsule
    draw.rounded_rectangle([cx, cy, cx + cw, cy + ch], radius=ch // 2, fill=(84, 52, 36, 255), outline=(212, 148, 74, 255), width=2)
    # "Hello" text
    draw.text((cx + 42, cy + 15), "HELLO", fill=(255, 255, 255, 255))
    # Small speech bubble tail
    draw.polygon([(cx + 40, cy + ch), (cx + 52, cy + ch), (cx + 36, cy + ch + 8)], fill=(84, 52, 36, 255))

    # Sparkle stars
    draw.line([(35, 115), (43, 115)], fill=(212, 148, 74, 255), width=2)
    draw.line([(39, 111), (39, 119)], fill=(212, 148, 74, 255), width=2)

    draw.line([(205, 110), (213, 110)], fill=(212, 148, 74, 255), width=2)
    draw.line([(209, 106), (209, 114)], fill=(212, 148, 74, 255), width=2)

    return im.convert("RGB")

def main():
    print("Generating animated 'Hello' language bubbles...")
    for i in range(4):
        im = draw_hello_frame(i)
        im.save(os.path.join(OUT_DIR, f"hello_cloud_{i}.bmp"))
    print("[+] Hello language frames generated successfully.")

if __name__ == "__main__":
    main()
