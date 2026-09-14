"""
Convert PPM screenshot to PNG and inspect screen text/content.
"""
import struct, os

def ppm_to_png(ppm_path, png_path):
    with open(ppm_path, 'rb') as f:
        header = f.readline().strip()
        if header != b'P6':
            print(f"Not a binary PPM: {header}")
            return
        dims = f.readline().strip()
        while dims.startswith(b'#'):
            dims = f.readline().strip()
        width, height = map(int, dims.split())
        max_val = int(f.readline().strip())
        raw_pixels = f.read()
    
    print(f"PPM parsed: {width}x{height}, max_val={max_val}, data size={len(raw_pixels)} bytes")
    
    # Simple PNG encoder in pure Python
    import zlib
    
    # Build raw image scanlines
    raw_data = bytearray()
    row_bytes = width * 3
    for y in range(height):
        raw_data.append(0) # filter type 0 (None)
        raw_data.extend(raw_pixels[y*row_bytes : (y+1)*row_bytes])
    
    compressed = zlib.compress(bytes(raw_data), 9)
    
    png = bytearray(b'\x89PNG\r\n\x1a\n')
    
    # IHDR
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    png.extend(struct.pack('>I', 13))
    png.extend(b'IHDR')
    png.extend(ihdr)
    png.extend(struct.pack('>I', zlib.crc32(b'IHDR' + ihdr) & 0xffffffff))
    
    # IDAT
    png.extend(struct.pack('>I', len(compressed)))
    png.extend(b'IDAT')
    png.extend(compressed)
    png.extend(struct.pack('>I', zlib.crc32(b'IDAT' + compressed) & 0xffffffff))
    
    # IEND
    png.extend(struct.pack('>I', 0))
    png.extend(b'IEND')
    png.extend(struct.pack('>I', zlib.crc32(b'IEND') & 0xffffffff))
    
    with open(png_path, 'wb') as out_f:
        out_f.write(png)
    print(f"[OK] Saved PNG: {png_path} ({len(png)} bytes)")

ppm1 = r'c:\Users\DanDevXP\Desktop\keshoos\out\qemu_screen.ppm'
ppm2 = r'c:\Users\DanDevXP\Desktop\keshoos\out\qemu_screen_boot.ppm'

if os.path.exists(ppm1): ppm_to_png(ppm1, r'c:\Users\DanDevXP\Desktop\keshoos\out\qemu_screen_menu.png')
if os.path.exists(ppm2): ppm_to_png(ppm2, r'c:\Users\DanDevXP\Desktop\keshoos\out\qemu_screen_boot.png')
