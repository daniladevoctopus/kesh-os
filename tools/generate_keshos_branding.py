import os
import shutil
from PIL import Image, ImageDraw, ImageFont

def create_gradient_h(width, height, c_left, c_right):
    im = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(im)
    for x in range(width):
        r = int(c_left[0] + (c_right[0] - c_left[0]) * (x / width))
        g = int(c_left[1] + (c_right[1] - c_left[1]) * (x / width))
        b = int(c_left[2] + (c_right[2] - c_left[2]) * (x / width))
        draw.line([(x, 0), (x, height)], fill=(r, g, b))
    return im

def create_gradient_v(width, height, c_top, c_bottom):
    im = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(im)
    for y in range(height):
        r = int(c_top[0] + (c_bottom[0] - c_top[0]) * (y / height))
        g = int(c_top[1] + (c_bottom[1] - c_top[1]) * (y / height))
        b = int(c_top[2] + (c_bottom[2] - c_top[2]) * (y / height))
        draw.line([(0, y), (width, y)], fill=(r, g, b))
    return im

repo = r'c:\Users\DanDevXP\Desktop\keshoos'
logo_path = os.path.join(repo, 'logo.png')
if not os.path.exists(logo_path):
    logo_path = os.path.join(repo, 'keshos_logo.png')
src_logo = Image.open(logo_path).convert("RGBA")

# 1. Main NT Banner (413 x 72)
def make_main_banner():
    im = create_gradient_h(413, 72, (222, 218, 212), (248, 246, 242))
    logo_size = 56
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (22, 8), logo_resized)
    
    draw = ImageDraw.Draw(im)
    font_main = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 30)
    font_sub = ImageFont.truetype(r"C:\Windows\Fonts\tahoma.ttf", 11)
    
    draw.text((91, 11), "KeshOS", fill=(210, 190, 175), font=font_main)
    draw.text((90, 10), "KeshOS", fill=(55, 35, 20), font=font_main)
    draw.text((91, 48), "NT Workstation 0.8.0 Beta \"Brownie\"", fill=(105, 80, 60), font=font_sub)
    return im

# 2. Separator line (413 x 5)
def make_separator_line():
    im = Image.new("RGB", (413, 5), (60, 35, 18))
    draw = ImageDraw.Draw(im)
    for x in range(413):
        factor = 1.0 - abs((x - 206.5) / 206.5)
        r = int(60 + (220 - 60) * factor)
        g = int(35 + (170 - 35) * factor)
        b = int(18 + (70 - 18) * factor)
        draw.point((x, 0), fill=(r // 2, g // 2, b // 2))
        draw.point((x, 1), fill=(r, g, b))
        draw.point((x, 2), fill=(min(255, r + 30), min(255, g + 30), min(255, b + 30)))
        draw.point((x, 3), fill=(r, g, b))
        draw.point((x, 4), fill=(min(255, r + 50), min(255, g + 50), min(255, b + 50)))
    return im

# 3. Small Flag / Mini Logo (48 x 40)
def make_flag():
    im = Image.new("RGB", (48, 40), (42, 42, 42))
    logo_size = 36
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (6, 2), logo_resized)
    return im

# 4. Welcome Title Banner (558 x 79)
def make_welcome_title():
    im = create_gradient_h(558, 79, (230, 224, 216), (250, 248, 244))
    draw = ImageDraw.Draw(im)
    for y in range(0, 79, 2):
        draw.line([(0, y), (558, y)], fill=(225, 218, 208))
    
    logo_size = 64
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (465, 8), logo_resized)
    
    font_large = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 34)
    font_sub = ImageFont.truetype(r"C:\Windows\Fonts\tahoma.ttf", 12)
    
    draw.text((26, 12), "KeshOS", fill=(215, 195, 175), font=font_large)
    draw.text((25, 11), "KeshOS", fill=(55, 35, 18), font=font_large)
    draw.text((26, 52), "Welcome to KeshOS NT 0.8.0 Beta \"Brownie\"", fill=(110, 85, 65), font=font_sub)
    return im

# 5. Setup Wizard Header (49 x 49)
def make_wizard_header():
    im = Image.new("RGB", (49, 49), (255, 255, 255))
    logo_size = 42
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (4, 4), logo_resized)
    return im

# 6. Setup Wizard Watermark (164 x 400, 8-bit palette mode P)
def make_wizard_watermark():
    im = create_gradient_v(164, 400, (45, 22, 10), (85, 42, 18))
    logo_size = 80
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (42, 28), logo_resized)
    
    draw = ImageDraw.Draw(im)
    font_title = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 18)
    font_sub = ImageFont.truetype(r"C:\Windows\Fonts\tahoma.ttf", 11)
    
    draw.text((50, 125), "KeshOS", fill=(235, 195, 80), font=font_title)
    draw.text((36, 150), "NT Workstation", fill=(220, 205, 190), font=font_sub)
    draw.text((45, 168), "Beta \"Brownie\"", fill=(185, 160, 135), font=font_sub)
    
    draw.line([(25, 365), (139, 365)], fill=(160, 110, 50))
    draw.line([(25, 366), (139, 366)], fill=(235, 195, 80))
    
    im_p = im.quantize(colors=256, method=Image.MAXCOVERAGE)
    return im_p

# 7. Boot Logo (190 x 224, 16-color 4-bit palette mode P)
def make_boot_logo():
    im = Image.new("RGB", (190, 224), (0, 0, 0))
    logo_size = 130
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (30, 25), logo_resized)
    
    draw = ImageDraw.Draw(im)
    font_main = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 26)
    font_sub = ImageFont.truetype(r"C:\Windows\Fonts\tahoma.ttf", 10)
    
    draw.text((48, 165), "KeshOS", fill=(255, 255, 255), font=font_main)
    draw.text((42, 200), "NT Workstation", fill=(210, 175, 75), font=font_sub)
    
    # Quantize to 16 colors
    im_16 = im.quantize(colors=16, method=Image.MAXCOVERAGE)
    return im_16

# 8. Rosbitmap & Mask (320 x 159) for LiveCD installer and SysDM System Properties
def make_rosbitmap_and_mask():
    # RGBA canvas
    canvas = Image.new("RGBA", (320, 159), (0, 0, 0, 0))

    # KeshOS Logo on the left
    logo_size = 78
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    canvas.paste(logo_resized, (22, (159 - logo_size) // 2), logo_resized)

    draw = ImageDraw.Draw(canvas)
    font_title = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 32)
    font_sub1 = ImageFont.truetype(r"C:\Windows\Fonts\tahoma.ttf", 12)
    font_sub2 = ImageFont.truetype(r"C:\Windows\Fonts\tahomabd.ttf", 11)

    # Shadow & Main text
    draw.text((115, 36), "KeshOS", fill=(215, 195, 175, 200), font=font_title)
    draw.text((114, 35), "KeshOS", fill=(52, 34, 18, 255), font=font_title)

    draw.text((115, 78), "NT Workstation 0.8.0 Beta", fill=(95, 72, 50, 255), font=font_sub1)
    draw.text((115, 96), "Codename \"Brownie\"", fill=(155, 105, 45, 255), font=font_sub2)

    # Decorative separator
    draw.line([(115, 118), (295, 118)], fill=(185, 145, 100, 220), width=1)

    # Base RGB image on neutral dialog gray
    ros_rgb = Image.new("RGB", (320, 159), (236, 233, 216))
    ros_rgb.paste(canvas, (0, 0), canvas)

    # Alpha mask (24-bit RGB where R=G=B=alpha, so GetPixel & 0xFF == alpha)
    alpha = canvas.split()[3]
    ros_mask = Image.merge("RGB", (alpha, alpha, alpha))

    return ros_rgb, ros_mask

# 9. LogonUI Logo (137 x 86)
def make_logonui_logo():
    bg_color = (90, 126, 220)
    im = Image.new("RGB", (137, 86), bg_color)
    logo_size = 46
    logo_resized = src_logo.resize((logo_size, logo_size), Image.LANCZOS)
    im.paste(logo_resized, (8, 16), logo_resized)

    draw = ImageDraw.Draw(im)
    font_title = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 19)
    font_sub = ImageFont.truetype(r"C:\Windows\Fonts\tahoma.ttf", 9)

    draw.text((60, 18), "KeshOS", fill=(255, 255, 255), font=font_title)
    draw.text((60, 44), "NT 0.8.0 Beta", fill=(225, 235, 255), font=font_sub)
    draw.text((60, 56), "\"Brownie\"", fill=(245, 210, 130), font=font_sub)
    return im

def apply_assets():
    main_banner = make_main_banner()
    sep_line = make_separator_line()
    flag = make_flag()
    welcome_title = make_welcome_title()
    wiz_hdr = make_wizard_header()
    wiz_watermark = make_wizard_watermark()
    boot_logo = make_boot_logo()
    ros_bmp, ros_mask = make_rosbitmap_and_mask()
    logonui_logo = make_logonui_logo()

    # Targets for main banner
    banner_targets = [
        os.path.join(repo, r'dll\win32\shell32\res\bitmaps\reactos.bmp'),
        os.path.join(repo, r'dll\win32\msgina\resources\reactos.bmp'),
        os.path.join(repo, r'dll\win32\syssetup\resources\reactos.bmp'),
    ]
    for t in banner_targets:
        if os.path.exists(t):
            main_banner.save(t, 'BMP')
            print(f'Applied banner -> {t}')

    # Targets for separator line
    line_targets = [
        os.path.join(repo, r'dll\win32\shell32\res\bitmaps\line.bmp'),
        os.path.join(repo, r'dll\win32\msgina\resources\line.bmp'),
        os.path.join(repo, r'dll\win32\syssetup\resources\line.bmp'),
    ]
    for t in line_targets:
        if os.path.exists(t):
            sep_line.save(t, 'BMP')
            print(f'Applied line -> {t}')

    # Targets for flag / mini logo
    flag_targets = [
        os.path.join(repo, r'dll\win32\shell32\res\bitmaps\reactos_flag.bmp'),
        os.path.join(repo, r'dll\win32\msgina\resources\reactos_flag.bmp'),
    ]
    for t in flag_targets:
        if os.path.exists(t):
            flag.save(t, 'BMP')
            print(f'Applied flag -> {t}')

    # Target for welcome title
    welcome_t = os.path.join(repo, r'base\setup\welcome\res\title.bmp')
    if os.path.exists(welcome_t):
        welcome_title.save(welcome_t, 'BMP')
        print(f'Applied welcome title -> {welcome_t}')

    # Targets for wizard header
    header_targets = [
        os.path.join(repo, r'dll\win32\syssetup\resources\header.bmp'),
        os.path.join(repo, r'base\setup\reactos\res\header.bmp'),
        os.path.join(repo, r'dll\cpl\hdwwiz\resources\header.bmp'),
        os.path.join(repo, r'dll\win32\newdev\resources\header.bmp'),
    ]
    for t in header_targets:
        if os.path.exists(t):
            wiz_hdr.save(t, 'BMP')
            print(f'Applied wizard header -> {t}')

    # Targets for wizard watermark
    watermark_targets = [
        os.path.join(repo, r'dll\win32\syssetup\resources\watermark.bmp'),
        os.path.join(repo, r'base\setup\reactos\res\watermark.bmp'),
        os.path.join(repo, r'dll\cpl\hdwwiz\resources\watermark.bmp'),
        os.path.join(repo, r'dll\win32\newdev\resources\watermark.bmp'),
    ]
    for t in watermark_targets:
        if os.path.exists(t):
            wiz_watermark.save(t, 'BMP')
            print(f'Applied watermark -> {t}')

    # Targets for boot logo
    boot_targets = [
        os.path.join(repo, r'ntoskrnl\inbv\resources\logo.bmp'),
        os.path.join(repo, r'ntoskrnl\inbv\resources\logo_xmas.bmp'),
    ]
    for t in boot_targets:
        if os.path.exists(t):
            boot_logo.save(t, 'BMP')
            print(f'Applied boot logo -> {t}')

    # Targets for rosbitmap and mask
    ros_bmp_targets = [
        os.path.join(repo, r'base\system\userinit\res\rosbitmap.bmp'),
        os.path.join(repo, r'dll\cpl\sysdm\resources\rosbitmap.bmp'),
    ]
    for t in ros_bmp_targets:
        if os.path.exists(t):
            ros_bmp.save(t, 'BMP')
            print(f'Applied rosbitmap -> {t}')

    ros_mask_targets = [
        os.path.join(repo, r'base\system\userinit\res\rosbitmap_mask.bmp'),
        os.path.join(repo, r'dll\cpl\sysdm\resources\rosbitmap_mask.bmp'),
    ]
    for t in ros_mask_targets:
        if os.path.exists(t):
            ros_mask.save(t, 'BMP')
            print(f'Applied rosbitmap_mask -> {t}')

    # Targets for logonui logo
    logon_t = os.path.join(repo, r'base\system\logonui\res\123.bmp')
    if os.path.exists(logon_t):
        logonui_logo.save(logon_t, 'BMP')
        print(f'Applied logonui logo -> {logon_t}')

    # Targets for KeshOS ICO (16, 32, 48)
    ico_targets = [
        os.path.join(repo, r'win32ss\user\user32\resources\oic_reactos.ico'),
        os.path.join(repo, r'base\applications\osk\res\reactos.ico'),
        os.path.join(repo, r'base\setup\reactos\res\roslogo.ico'),
        os.path.join(repo, r'base\setup\reactos\res\roslogo_2.ico'),
        os.path.join(repo, r'base\shell\progman\res\roslogo.ico'),
    ]
    for t in ico_targets:
        if os.path.exists(t):
            src_logo.save(t, format='ICO', sizes=[(16, 16), (32, 32), (48, 48)])
            print(f'Applied KeshOS ICO -> {t}')

if __name__ == '__main__':
    apply_assets()
    print('All KeshOS branding plates applied successfully!')

