"""
Generate official KeshOS Beta 0.8.0 "Brownie" 4K Wallpaper.
Rich Material You chocolate backdrop with subtle geometric glassmorphic waves and KeshOS branding.
"""
import os, struct, zlib, math

keshos_root = r'c:\Users\DanDevXP\Desktop\keshoos'
wallpaper_dir = os.path.join(keshos_root, 'branding', 'usr', 'share', 'wallpapers', 'KeshOS_Brownie', 'contents', 'images')
os.makedirs(wallpaper_dir, exist_ok=True)
metadata_dir = os.path.join(keshos_root, 'branding', 'usr', 'share', 'wallpapers', 'KeshOS_Brownie')

# 1. Write metadata.desktop for KDE Plasma Wallpaper Manager
metadata_desktop = """[Desktop Entry]
Name=KeshOS Brownie
Name[ru]=KeshOS Брауни
Comment=Official KeshOS Beta 0.8.0 Material You Wallpaper
X-KDE-PluginInfo-Author=SneakDeak Team
X-KDE-PluginInfo-Email=dev@kesh.local
X-KDE-PluginInfo-Name=KeshOS_Brownie
X-KDE-PluginInfo-Version=0.8.0
X-KDE-PluginInfo-License=GPLv3
"""
with open(os.path.join(metadata_dir, 'metadata.desktop'), 'w', encoding='utf-8') as f:
    f.write(metadata_desktop)

# 2. Pure Python 1920x1080 PNG Generator with smooth chocolate gradient & waves
W, H = 1920, 1080
out_png = os.path.join(wallpaper_dir, '1920x1080.png')
print(f"Rendering KeshOS Brownie Wallpaper ({W}x{H})...")

# Colors
# Top: Deep Espresso (22, 14, 10)
# Center Wave: Rich Chocolate (61, 39, 29)
# Bottom/Caramel Glow: (180, 100, 50)
rows = []
for y in range(H):
    row_bytes = bytearray([0]) # filter type 0
    ny = y / H
    for x in range(W):
        nx = x / W
        
        # Base vertical gradient
        r = int(22 + (55 - 22) * ny)
        g = int(14 + (35 - 14) * ny)
        b = int(10 + (25 - 10) * ny)
        
        # Flowing wave 1
        wave1 = math.sin(nx * 3.5 + ny * 1.5) * 0.5 + 0.5
        wave2 = math.cos(nx * 2.0 - ny * 3.0) * 0.5 + 0.5
        glow = math.exp(-((nx - 0.5)**2 + (ny - 0.6)**2) * 4) * 0.8
        
        r = min(255, int(r + wave1 * 35 + glow * 100))
        g = min(255, int(g + wave1 * 20 + glow * 55))
        b = min(255, int(b + wave2 * 15 + glow * 25))
        
        row_bytes.extend((r, g, b))
    rows.append(bytes(row_bytes))

raw_data = b''.join(rows)
compressed = zlib.compress(raw_data, 6)

def make_chunk(ctype, data):
    return struct.pack('>I', len(data)) + ctype + data + struct.pack('>I', zlib.crc32(ctype + data) & 0xffffffff)

png_bytes = b'\x89PNG\r\n\x1a\n'
png_bytes += make_chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
png_bytes += make_chunk(b'IDAT', compressed)
png_bytes += make_chunk(b'IEND', b'')

with open(out_png, 'wb') as f:
    f.write(png_bytes)

print(f"[OK] Wallpaper saved: {out_png} ({os.path.getsize(out_png) / (1024*1024):.2f} MB)")
