#!/usr/bin/env python3
"""
Generate Material You style assets for KeshOS NT 0.8.0 OOBE.
Palette: Warm chocolate, cocoa, amber, soft cream, and Material You pastels.
"""

import math
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

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

def draw_waving_character(frame_idx, total_frames=4):
    im = Image.new("RGBA", (WIDTH, HEIGHT), (255, 255, 255, 255))
    draw = ImageDraw.Draw(im)

    create_circular_background(draw, (120, 120), 105, (245, 235, 225), (255, 248, 240))
    draw.ellipse([70, 205, 170, 222], fill=(210, 195, 185, 255))

    draw.rounded_rectangle([75, 130, 165, 210], radius=35, fill=(84, 52, 36, 255))
    draw.line([120, 145, 120, 205], fill=(212, 148, 74, 255), width=3)

    draw.ellipse([82, 60, 158, 136], fill=(255, 228, 204, 255))
    draw.chord([82, 58, 158, 115], 180, 360, fill=(62, 38, 24, 255))
    draw.ellipse([110, 52, 130, 68], fill=(212, 148, 74, 255))

    draw.arc([98, 92, 112, 104], 180, 360, fill=(50, 30, 20, 255), width=3)
    draw.arc([128, 92, 142, 104], 180, 360, fill=(50, 30, 20, 255), width=3)
    draw.arc([112, 106, 128, 120], 0, 180, fill=(180, 70, 60, 255), width=3)
    draw.ellipse([93, 102, 103, 110], fill=(255, 180, 180, 255))
    draw.ellipse([137, 102, 147, 110], fill=(255, 180, 180, 255))

    draw.ellipse([64, 150, 82, 180], fill=(84, 52, 36, 255))
    draw.ellipse([62, 172, 78, 188], fill=(255, 228, 204, 255))

    wave_angles = [-15, 5, 25, 5]
    angle = wave_angles[frame_idx % len(wave_angles)]
    rad = math.radians(angle)

    shoulder_x, shoulder_y = 155, 145
    arm_len = 36
    elbow_x = shoulder_x + int(arm_len * math.cos(math.radians(-30)))
    elbow_y = shoulder_y + int(arm_len * math.sin(math.radians(-30)))

    forearm_len = 38
    hand_x = elbow_x + int(forearm_len * math.sin(rad))
    hand_y = elbow_y - int(forearm_len * math.cos(rad))

    draw.line([shoulder_x, shoulder_y, elbow_x, elbow_y], fill=(84, 52, 36, 255), width=16)
    draw.line([elbow_x, elbow_y, hand_x, hand_y], fill=(84, 52, 36, 255), width=14)
    draw.ellipse([hand_x - 12, hand_y - 12, hand_x + 12, hand_y + 12], fill=(255, 228, 204, 255))
    draw.ellipse([hand_x - 4, hand_y - 16, hand_x + 6, hand_y - 6], fill=(255, 228, 204, 255))

    sparkle_x = hand_x + 16
    draw.arc([sparkle_x - 6, hand_y - 12, sparkle_x + 6, hand_y], 270, 90, fill=(212, 148, 74, 255), width=2)
    draw.arc([sparkle_x - 2, hand_y - 18, sparkle_x + 12, hand_y - 6], 270, 90, fill=(212, 148, 74, 255), width=2)

    return im.convert("RGB")

def draw_globe_network(frame_idx, total_frames=4):
    im = Image.new("RGBA", (WIDTH, HEIGHT), (255, 255, 255, 255))
    draw = ImageDraw.Draw(im)

    create_circular_background(draw, (120, 120), 105, (238, 242, 250), (250, 252, 255))
    draw.ellipse([70, 205, 170, 222], fill=(200, 205, 215, 255))

    gx, gy, gr = 120, 120, 68
    draw.ellipse([gx - gr, gy - gr, gx + gr, gy + gr], fill=(58, 92, 145, 255))
    draw.ellipse([gx - 45, gy - 35, gx - 5, gy + 15], fill=(116, 175, 142, 255))
    draw.ellipse([gx + 10, gy - 40, gx + 50, gy - 5], fill=(116, 175, 142, 255))
    draw.ellipse([gx - 15, gy + 10, gx + 35, gy + 50], fill=(116, 175, 142, 255))

    draw.arc([gx - gr, gy - gr, gx + gr, gy + gr], 90, 270, fill=(255, 255, 255, 255), width=4)

    t = (frame_idx % total_frames) / total_frames
    orbit_phase = t * math.pi * 2

    draw.arc([gx - 85, gy - 35, gx + 85, gy + 35], 0, 360, fill=(212, 148, 74, 255), width=3)
    sat1_x = gx + int(82 * math.cos(orbit_phase))
    sat1_y = gy + int(32 * math.sin(orbit_phase))
    draw.ellipse([sat1_x - 7, sat1_y - 7, sat1_x + 7, sat1_y + 7], fill=(255, 200, 100, 255))

    pulse_r = int(76 + (t * 18))
    draw.arc([gx - pulse_r, gy - pulse_r, gx + pulse_r, gy + pulse_r], 210, 330, fill=(76, 175, 80, 255), width=4)

    draw.rounded_rectangle([150, 145, 200, 195], radius=12, fill=(84, 52, 36, 255))
    draw.rectangle([165, 137, 171, 145], fill=(212, 148, 74, 255))
    draw.rectangle([179, 137, 185, 145], fill=(212, 148, 74, 255))
    draw.ellipse([170, 165, 180, 175], fill=(76, 217, 100, 255))

    return im.convert("RGB")

def draw_eula_shield():
    im = Image.new("RGBA", (WIDTH, HEIGHT), (255, 255, 255, 255))
    draw = ImageDraw.Draw(im)

    create_circular_background(draw, (120, 120), 105, (250, 245, 238), (255, 252, 248))
    draw.ellipse([70, 205, 170, 222], fill=(215, 205, 195, 255))

    draw.rounded_rectangle([75, 55, 145, 175], radius=10, fill=(255, 255, 255, 255), outline=(220, 205, 190, 255), width=2)
    for y in range(80, 150, 14):
        draw.rounded_rectangle([88, y, 132, y + 4], radius=2, fill=(200, 180, 165, 255))

    sx, sy = 145, 125
    shield_pts = [
        (sx - 38, sy - 45),
        (sx + 38, sy - 45),
        (sx + 38, sy + 5),
        (sx, sy + 45),
        (sx - 38, sy + 5)
    ]
    draw.polygon(shield_pts, fill=(84, 52, 36, 255))
    inner_pts = [
        (sx - 32, sy - 40),
        (sx + 32, sy - 40),
        (sx + 32, sy + 3),
        (sx, sy + 38),
        (sx - 32, sy + 3)
    ]
    draw.polygon(inner_pts, outline=(212, 148, 74, 255), width=3)

    check_pts = [
        (sx - 18, sy),
        (sx - 6, sy + 15),
        (sx + 18, sy - 14)
    ]
    draw.line(check_pts, fill=(255, 255, 255, 255), width=6, joint="curve")

    return im.convert("RGB")

def draw_theme_previews():
    card_w, card_h = 160, 110
    
    im_b = Image.new("RGBA", (card_w, card_h), (255, 255, 255, 255))
    d_b = ImageDraw.Draw(im_b)
    d_b.rounded_rectangle([0, 0, card_w - 1, card_h - 1], radius=10, fill=(45, 26, 16, 255), outline=(138, 84, 53, 255), width=2)
    d_b.rectangle([0, card_h - 22, card_w, card_h], fill=(36, 20, 12, 255))
    d_b.rounded_rectangle([4, card_h - 19, 32, card_h - 3], radius=4, fill=(108, 66, 42, 255), outline=(212, 148, 74, 255), width=1)
    d_b.rounded_rectangle([15, 12, card_w - 20, card_h - 30], radius=5, fill=(250, 246, 242, 255), outline=(78, 46, 30, 255), width=1)
    d_b.rectangle([15, 12, card_w - 20, 28], fill=(68, 40, 26, 255))
    d_b.ellipse([card_w - 30, 16, card_w - 24, 22], fill=(200, 70, 60, 255))

    im_c = Image.new("RGBA", (card_w, card_h), (255, 255, 255, 255))
    d_c = ImageDraw.Draw(im_c)
    d_c.rounded_rectangle([0, 0, card_w - 1, card_h - 1], radius=10, fill=(0, 128, 128, 255), outline=(160, 160, 160, 255), width=2)
    d_c.rectangle([0, card_h - 22, card_w, card_h], fill=(192, 192, 192, 255))
    d_c.rectangle([3, card_h - 19, 30, card_h - 4], fill=(212, 208, 200, 255), outline=(128, 128, 128, 255), width=1)
    d_c.rectangle([15, 12, card_w - 20, card_h - 30], fill=(236, 233, 216, 255), outline=(100, 100, 100, 255), width=1)
    d_c.rectangle([16, 13, card_w - 21, 26], fill=(10, 36, 106, 255))

    return im_b.convert("RGB"), im_c.convert("RGB")

def draw_account_avatar():
    im = Image.new("RGBA", (WIDTH, HEIGHT), (255, 255, 255, 255))
    draw = ImageDraw.Draw(im)

    create_circular_background(draw, (120, 120), 105, (245, 238, 232), (255, 250, 245))
    draw.ellipse([70, 205, 170, 222], fill=(215, 205, 195, 255))

    draw.ellipse([45, 45, 195, 195], fill=(84, 52, 36, 255))
    draw.ellipse([50, 50, 190, 190], fill=(255, 245, 238, 255))

    draw.ellipse([92, 70, 148, 126], fill=(212, 148, 74, 255))
    draw.chord([68, 132, 172, 210], 180, 360, fill=(84, 52, 36, 255))

    draw.ellipse([148, 148, 188, 188], fill=(76, 175, 80, 255), outline=(255, 255, 255, 255), width=3)
    draw.line([(158, 168), (166, 176), (178, 160)], fill=(255, 255, 255, 255), width=3)

    return im.convert("RGB")

def main():
    print(f"Generating Material You assets in {OUT_DIR}...")
    
    for i in range(4):
        mascot = draw_waving_character(i)
        mascot.save(os.path.join(OUT_DIR, f"mascot_wave_{i}.bmp"))
    print("[+] Waving mascot frames generated.")

    for i in range(4):
        globe = draw_globe_network(i)
        globe.save(os.path.join(OUT_DIR, f"globe_net_{i}.bmp"))
    print("[+] Network globe frames generated.")

    shield = draw_eula_shield()
    shield.save(os.path.join(OUT_DIR, "eula_shield.bmp"))
    print("[+] EULA shield generated.")

    tb, tc = draw_theme_previews()
    tb.save(os.path.join(OUT_DIR, "theme_brownie.bmp"))
    tc.save(os.path.join(OUT_DIR, "theme_classic.bmp"))
    print("[+] Theme preview cards generated.")

    avatar = draw_account_avatar()
    avatar.save(os.path.join(OUT_DIR, "account_avatar.bmp"))
    print("[+] Account avatar generated.")

    print("All Material You assets ready!")

if __name__ == "__main__":
    main()
