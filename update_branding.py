from PIL import Image

def main():
    # 1. Wallpapers
    wall = Image.open('keshos.bmp').convert('RGB')
    wall_scaled = wall.resize((1600, 893), Image.LANCZOS)
    wall_scaled.save('src/gui/wallpaper/wallpaper.bmp')
    wall_scaled.save('src/gui/wallpaper/wallpaper_day.bmp')
    wall_scaled.save('src/gui/wallpaper/wallpaper_night.bmp')
    print('Wallpapers updated successfully: 1600x893 BMP')

    # 2. About Window Logo
    # сохраняем чистый 32-битный логотип без кругов и черных рамок


    # 3. Boot Logo (200x200 RGB C array)
    boot_logo = Image.open('keshos_logo.png').convert('RGBA')
    boot_logo.thumbnail((200, 200), Image.LANCZOS)
    bg = Image.new('RGB', (200, 200), (0, 0, 0))
    bx = (200 - boot_logo.width) // 2
    by = (200 - boot_logo.height) // 2
    bg.paste(boot_logo, (bx, by), boot_logo)

    with open('boot/loading/load_logo.c', 'w', encoding='utf-8') as f:
        f.write('#include "load_logo.h"\n\n')
        f.write('const unsigned char load_logo[LOGO_WIDTH * LOGO_HEIGHT * 3] = {\n')
        data = bg.tobytes()
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_vals = ', '.join(f'0x{b:02X}' for b in chunk)
            f.write(f'    {hex_vals},\n')
        f.write('};\n')
    print('Boot logo updated successfully: 200x200 in load_logo.c')

if __name__ == '__main__':
    main()
