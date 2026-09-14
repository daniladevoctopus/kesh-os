import os
from PIL import Image, ImageDraw

def refine_theme():
    repo = r'c:\Users\DanDevXP\Desktop\keshoos'
    bmp_dir = os.path.join(repo, r'media\themes\Brownie\brownie.msstyles\bitmaps')
    logo_path = os.path.join(repo, 'logo.png')
    src_logo = Image.open(logo_path).convert('RGBA')

    # ==========================================
    # 1. Start Button: NORMAL_STARTBUTTON.bmp & .png (43 x 75)
    # ==========================================
    start_btn = Image.new('RGB', (43, 75), (255, 0, 255))
    start_btn_rgba = Image.new('RGBA', (43, 75), (255, 0, 255, 0))

    def make_start_button_state(y_offset, is_hot=False, is_pressed=False):
        im = Image.new('RGBA', (43, 25), (255, 0, 255, 0))
        draw = ImageDraw.Draw(im)

        bx0, by0, bx1, by1 = 3, 2, 39, 22
        if is_pressed:
            bx0 += 1
            by0 += 1
            bx1 += 1
            by1 += 1

        if is_hot:
            border_col = (230, 175, 90, 255) # Bright golden amber
            bg_top = (110, 68, 42, 255)
            bg_bot = (75, 44, 26, 255)
        elif is_pressed:
            border_col = (110, 68, 40, 255)
            bg_top = (42, 23, 14, 255)
            bg_bot = (60, 34, 20, 255)
        else: # Normal
            border_col = (130, 82, 52, 255)
            bg_top = (85, 50, 31, 255)
            bg_bot = (54, 30, 18, 255)

        # Draw smooth rounded capsule pill
        draw.rounded_rectangle([bx0, by0, bx1, by1], radius=5, fill=bg_bot, outline=border_col, width=1)
        # Bevel highlight on top
        draw.line([(bx0 + 2, by0 + 1), (bx1 - 2, by0 + 1)], fill=bg_top)

        l_size = 18
        logo_res = src_logo.resize((l_size, l_size), Image.LANCZOS)
        lx = (43 - l_size) // 2 + (1 if is_pressed else 0)
        ly = (25 - l_size) // 2 + (1 if is_pressed else 0)

        if is_hot:
            # GLOWING AURA behind logo!
            glow_size = l_size + 8
            glow = Image.new('RGBA', (glow_size, glow_size), (0, 0, 0, 0))
            gdraw = ImageDraw.Draw(glow)
            gc = glow_size // 2
            for r in range(glow_size // 2, 0, -1):
                alpha = int(120 * (1.0 - r / (glow_size / 2.0)))
                gdraw.ellipse([gc - r, gc - r, gc + r, gc + r], fill=(255, 220, 120, alpha))
            im.paste(glow, (lx - 4, ly - 4), glow)

            # Brightened logo
            bright_logo = logo_res.copy()
            pixels = bright_logo.load()
            for py in range(l_size):
                for px in range(l_size):
                    r, g, b, a = pixels[px, py]
                    if a > 0:
                        r = min(255, int(r * 1.35 + 25))
                        g = min(255, int(g * 1.35 + 25))
                        b = min(255, int(b * 1.35 + 25))
                        pixels[px, py] = (r, g, b, a)
            im.paste(bright_logo, (lx, ly), bright_logo)
        else:
            im.paste(logo_res, (lx, ly), logo_res)

        # Transfer to canvas with exact magenta transparent color
        for cy in range(25):
            for cx in range(43):
                r, g, b, a = im.getpixel((cx, cy))
                if a > 10:
                    start_btn.putpixel((cx, y_offset + cy), (r, g, b))
                    start_btn_rgba.putpixel((cx, y_offset + cy), (r, g, b, a))
                else:
                    start_btn.putpixel((cx, y_offset + cy), (255, 0, 255))
                    start_btn_rgba.putpixel((cx, y_offset + cy), (255, 0, 255, 0))

    make_start_button_state(0, is_hot=False, is_pressed=False)
    make_start_button_state(25, is_hot=True, is_pressed=False)
    make_start_button_state(50, is_hot=False, is_pressed=True)

    start_btn.save(os.path.join(bmp_dir, 'NORMAL_STARTBUTTON.bmp'))
    start_btn_rgba.save(os.path.join(bmp_dir, 'NORMAL_STARTBUTTON.png'))
    print("Updated NORMAL_STARTBUTTON.bmp & .png with glowing KeshOS logo on hover!")

    # ==========================================
    # 2. Refined Taskbar Background (50 x 26)
    # ==========================================
    tb_bg = Image.new('RGB', (50, 26))
    tb_draw = ImageDraw.Draw(tb_bg)
    # y=0: Bevel highlight line (golden cocoa)
    tb_draw.line([(0, 0), (50, 0)], fill=(138, 86, 56))
    # y=1: Inner soft glow
    tb_draw.line([(0, 1), (50, 1)], fill=(108, 64, 42))
    # y=2..24: Rich cocoa gradient
    for y in range(2, 25):
        t = (y - 2) / 22.0
        r = int(84 - 28 * t)
        g = int(50 - 18 * t)
        b = int(32 - 12 * t)
        tb_draw.line([(0, y), (50, y)], fill=(r, g, b))
    # y=25: Bottom shadow line
    tb_draw.line([(0, 25), (50, 25)], fill=(36, 20, 12))

    tb_bg.save(os.path.join(bmp_dir, 'NORMAL_TASKBARBACKGROUND.bmp'))
    print("Updated NORMAL_TASKBARBACKGROUND.bmp with warm cocoa bevel!")

    # ==========================================
    # 3. Refined Taskbar Tray (110 x 26)
    # ==========================================
    tray_bg = Image.new('RGB', (110, 26))
    tray_draw = ImageDraw.Draw(tray_bg)
    # Left divider groove
    tray_draw.line([(0, 2), (0, 24)], fill=(40, 22, 14))
    tray_draw.line([(1, 2), (1, 24)], fill=(110, 68, 44))
    # Horizontal gradient matching taskbar with slight groove depth
    tray_draw.line([(0, 0), (110, 0)], fill=(138, 86, 56))
    tray_draw.line([(0, 1), (110, 1)], fill=(100, 58, 38))
    for y in range(2, 25):
        t = (y - 2) / 22.0
        r = int(76 - 26 * t)
        g = int(45 - 17 * t)
        b = int(28 - 11 * t)
        tray_draw.line([(2, y), (110, y)], fill=(r, g, b))
    tray_draw.line([(0, 25), (110, 25)], fill=(34, 18, 11))

    tray_bg.save(os.path.join(bmp_dir, 'NORMAL_TASKBARTRAY.bmp'))
    print("Updated NORMAL_TASKBARTRAY.bmp with matching chocolate groove!")

if __name__ == '__main__':
    refine_theme()
