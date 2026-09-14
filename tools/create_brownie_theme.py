import os
import shutil
from PIL import Image

def colorize_to_chocolate(im):
    # Convert image to RGBA
    orig_mode = im.mode
    im_rgba = im.convert("RGBA")
    r_ch, g_ch, b_ch, a_ch = im_rgba.split()
    
    # Calculate luminance of each pixel
    # Map luminance 0..255 to a chocolate ramp:
    # 0 (darkest):   RGB(42, 24, 15)   (deep dark chocolate)
    # 64 (dark mid): RGB(78, 46, 29)   (mocha)
    # 128 (mid):     RGB(120, 80, 55)  (milk chocolate)
    # 192 (light):   RGB(195, 172, 150)(caramel / latte)
    # 255 (white):   RGB(250, 246, 240)(cream / white)
    
    ramp = []
    for i in range(256):
        t = i / 255.0
        if t < 0.25:
            # 0.0 to 0.25: (42, 24, 15) to (78, 46, 29)
            k = t / 0.25
            r = int(42 + (78 - 42) * k)
            g = int(24 + (46 - 24) * k)
            b = int(15 + (29 - 15) * k)
        elif t < 0.50:
            # 0.25 to 0.50: (78, 46, 29) to (120, 80, 55)
            k = (t - 0.25) / 0.25
            r = int(78 + (120 - 78) * k)
            g = int(46 + (80 - 46) * k)
            b = int(29 + (55 - 29) * k)
        elif t < 0.75:
            # 0.50 to 0.75: (120, 80, 55) to (195, 172, 150)
            k = (t - 0.50) / 0.25
            r = int(120 + (195 - 120) * k)
            g = int(80 + (172 - 80) * k)
            b = int(55 + (150 - 55) * k)
        else:
            # 0.75 to 1.00: (195, 172, 150) to (250, 246, 240)
            k = (t - 0.75) / 0.25
            r = int(195 + (250 - 195) * k)
            g = int(172 + (246 - 172) * k)
            b = int(150 + (240 - 150) * k)
        ramp.append((r, g, b))
        
    out = Image.new("RGBA", im.size)
    out_pixels = out.load()
    in_pixels = im_rgba.load()
    
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = in_pixels[x, y]
            if a == 0:
                out_pixels[x, y] = (0, 0, 0, 0)
                continue
            lum = int(0.299 * r + 0.587 * g + 0.114 * b)
            cr, cg, cb = ramp[lum]
            out_pixels[x, y] = (cr, cg, cb, a)
            
    if orig_mode == "RGB":
        return out.convert("RGB")
    elif orig_mode == "P":
        return out.convert("RGB").quantize(colors=256)
    return out

def create_brownie_theme():
    repo = r'c:\Users\DanDevXP\Desktop\keshoos'
    src_dir = os.path.join(repo, r'media\themes\Lautus')
    dst_dir = os.path.join(repo, r'media\themes\Brownie')
    
    if os.path.exists(dst_dir):
        shutil.rmtree(dst_dir)
        
    print(f"Copying {src_dir} to {dst_dir}...")
    shutil.copytree(src_dir, dst_dir)
    
    # 1. Rename root CMakeLists.txt and folder structure
    msstyles_dir = os.path.join(dst_dir, 'brownie.msstyles')
    os.rename(os.path.join(dst_dir, 'lautus.msstyles'), msstyles_dir)
    os.rename(os.path.join(msstyles_dir, 'lautus.rc'), os.path.join(msstyles_dir, 'brownie.rc'))
    
    # 2. Update media/themes/Brownie/CMakeLists.txt
    cmake_root = os.path.join(dst_dir, 'CMakeLists.txt')
    with open(cmake_root, 'w', encoding='utf-8') as f:
        f.write('add_subdirectory(brownie.msstyles)\n')
        
    # 3. Update brownie.msstyles/CMakeLists.txt
    cmake_msstyles = os.path.join(msstyles_dir, 'CMakeLists.txt')
    with open(cmake_msstyles, 'r', encoding='utf-8') as f:
        c_content = f.read()
    c_content = c_content.replace('lautus.rc', 'brownie.rc')
    c_content = c_content.replace('lautus.msstyles', 'brownie.msstyles')
    c_content = c_content.replace('Lautus', 'Brownie')
    with open(cmake_msstyles, 'w', encoding='utf-8') as f:
        f.write(c_content)
        
    # 4. Update brownie.rc
    brownie_rc = os.path.join(msstyles_dir, 'brownie.rc')
    with open(brownie_rc, 'r', encoding='utf-8') as f:
        rc_content = f.read()
    rc_content = rc_content.replace('Lautus', 'Brownie')
    rc_content = rc_content.replace('lautus', 'brownie')
    with open(brownie_rc, 'w', encoding='utf-8') as f:
        f.write(rc_content)
        
    # 5. Update lang files
    lang_dir = os.path.join(msstyles_dir, 'lang')
    for lf in os.listdir(lang_dir):
        lpath = os.path.join(lang_dir, lf)
        with open(lpath, 'r', encoding='utf-8', errors='ignore') as f:
            l_content = f.read()
        l_content = l_content.replace('Lautus', 'Brownie')
        with open(lpath, 'w', encoding='utf-8') as f:
            f.write(l_content)
            
    # 6. Update textfiles (NormalNormal.INI, etc.) with Chocolate SysMetrics
    text_dir = os.path.join(msstyles_dir, 'textfiles')
    for tf in os.listdir(text_dir):
        tpath = os.path.join(text_dir, tf)
        with open(tpath, 'r', encoding='utf-8') as f:
            t_content = f.read()
            
        # Customize SysMetrics
        t_content = t_content.replace('ActiveCaption = 21 21 21', 'ActiveCaption = 56 34 22')
        t_content = t_content.replace('GradientActiveCaption = 58 58 58', 'GradientActiveCaption = 104 66 45')
        t_content = t_content.replace('CaptionText = 255 255 255', 'CaptionText = 252 248 244')
        t_content = t_content.replace('InactiveCaption = 65 65 65', 'InactiveCaption = 95 82 74')
        t_content = t_content.replace('GradientInactiveCaption = 81 81 81', 'GradientInactiveCaption = 135 120 110')
        t_content = t_content.replace('InactiveCaptionText = 180 180 180', 'InactiveCaptionText = 215 205 198')
        t_content = t_content.replace('Btnface = 239 238 243', 'Btnface = 238 233 226')
        t_content = t_content.replace('Background = 73 73 73', 'Background = 150 120 95')
        t_content = t_content.replace('Highlight = 75 75 75', 'Highlight = 85 52 34')
        t_content = t_content.replace('MenuHilight = 100 100 100', 'MenuHilight = 110 72 48')
        t_content = t_content.replace('MenuText = 75 75 75', 'MenuText = 50 32 20')
        t_content = t_content.replace('MenuBar = 239 238 243', 'MenuBar = 238 233 226')
        t_content = t_content.replace('EdgeFillColor = 239 238 243', 'EdgeFillColor = 238 233 226')
        
        with open(tpath, 'w', encoding='utf-8') as f:
            f.write(t_content)
            
    # 7. Colorize bitmaps
    import io
    bmp_dir = os.path.join(msstyles_dir, 'bitmaps')
    print("Colorizing bitmaps to chocolate tones...")
    for bf in os.listdir(bmp_dir):
        if bf.lower().endswith('.bmp'):
            bpath = os.path.join(bmp_dir, bf)
            try:
                with open(bpath, 'rb') as f:
                    buf = io.BytesIO(f.read())
                with Image.open(buf) as im:
                    choc_im = colorize_to_chocolate(im)
                choc_im.save(bpath)
            except Exception as e:
                print(f"Skipping {bf}: {e}")
                
    print("Brownie theme generated successfully!")

if __name__ == '__main__':
    create_brownie_theme()
