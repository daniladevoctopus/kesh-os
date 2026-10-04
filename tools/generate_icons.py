"""Build the small, neutral KeshOS icon set as 48px 32-bit BMP-backed ICO files."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import struct

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets' / 'icons'
OUT.mkdir(parents=True, exist_ok=True)
S = 48

def ico_write(image, name):
    image = image.convert('RGBA').resize((S, S), Image.Resampling.LANCZOS)
    raw = bytearray()
    # ICO DIB rows are bottom-up and use BGRA bytes.
    for y in range(S - 1, -1, -1):
        for x in range(S):
            r, g, b, a = image.getpixel((x, y))
            raw += bytes((b, g, r, a))
    mask_stride = ((S + 31) // 32) * 4
    and_mask = bytes(mask_stride * S)
    dib = struct.pack('<IIIHHIIIIII', 40, S, S * 2, 1, 32, 0, len(raw), 0, 0, 0, 0) + raw + and_mask
    header = struct.pack('<HHH', 0, 1, 1)
    entry = struct.pack('<BBBBHHII', S, S, 0, 0, 1, 32, len(dib), 22)
    (OUT / f'{name}.ico').write_bytes(header + entry + dib)

def canvas(): return Image.new('RGBA', (S, S), (0, 0, 0, 0))
def stroke(draw, pts, fill=(73, 82, 96, 255), width=3): draw.line(pts, fill=fill, width=width, joint='curve')
def rounded(draw, xy, radius=7, fill=(238, 240, 243, 255), outline=(130, 139, 151, 255), width=2): draw.rounded_rectangle(xy, radius, fill=fill, outline=outline, width=width)

def simple(kind):
    im = canvas(); d = ImageDraw.Draw(im)
    ink=(70,80,94,255); soft=(232,235,239,255); mid=(160,169,180,255); blue=(99,125,150,255); warm=(166,126,86,255)
    if kind == 'folder':
        d.rounded_rectangle((5,14,43,38), 6, fill=(225,213,190,255), outline=warm, width=2)
        d.rounded_rectangle((8,10,25,19), 4, fill=(239,227,203,255), outline=warm, width=2)
        d.line((7,22,41,22), fill=(245,238,220,255), width=2)
    elif kind == 'explorer':
        rounded(d,(6,6,42,42)); d.rectangle((8,13,40,15), fill=blue); d.rectangle((12,20,36,22),fill=mid); d.rectangle((12,27,32,29),fill=mid); d.rectangle((12,34,27,36),fill=mid)
        d.ellipse((11,8,13,10),fill=(255,255,255,255)); d.ellipse((16,8,18,10),fill=(255,255,255,255))
    elif kind == 'terminal':
        rounded(d,(5,8,43,40),fill=(49,56,64,255),outline=(96,108,120,255)); stroke(d,[(13,18),(19,23),(13,28)],(235,238,241,255),2); stroke(d,[(23,29),(33,29)],(197,205,213,255),2)
    elif kind == 'taskmgr':
        rounded(d,(6,6,42,42)); d.rectangle((11,29,16,36),fill=blue); d.rectangle((21,20,26,36),fill=(115,139,116,255)); d.rectangle((31,12,36,36),fill=warm); stroke(d,[(10,16),(17,22),(23,18),(29,25),(38,12)],ink,2)
    elif kind == 'browser':
        d.ellipse((6,6,42,42),fill=soft,outline=blue,width=2); d.ellipse((13,13,35,35),outline=blue,width=2); stroke(d,[(7,24),(41,24)],blue,2); stroke(d,[(24,7),(24,41)],blue,2); d.arc((13,7,35,41),90,270,fill=blue,width=2); d.arc((13,7,35,41),270,90,fill=blue,width=2)
    elif kind == 'installer':
        rounded(d,(7,5,41,43)); d.rectangle((11,10,37,31),fill=(214,223,231,255),outline=mid,width=1); d.rectangle((14,13,34,16),fill=blue); d.rectangle((14,20,26,22),fill=mid); d.polygon([(27,34),(32,39),(38,31),(36,29),(32,34),(29,31)],fill=(92,126,98,255))
    elif kind == 'notepad':
        rounded(d,(9,5,39,43),4); d.rectangle((13,12,35,14),fill=blue); d.line((13,20,34,20),fill=mid,width=2); d.line((13,26,32,26),fill=mid,width=2); d.line((13,32,29,32),fill=mid,width=2)
    elif kind == 'paint':
        d.ellipse((6,8,40,41),fill=(236,226,214,255),outline=warm,width=2); d.ellipse((16,14,20,18),fill=(202,112,96,255)); d.ellipse((26,13,30,17),fill=(104,137,174,255)); d.ellipse((13,24,17,28),fill=(105,148,116,255)); d.ellipse((24,32,29,37),fill=(255,255,255,0)); stroke(d,[(34,11),(40,6)],ink,3)
    elif kind == 'doom':
        d.polygon([(24,5),(39,14),(36,34),(24,43),(12,34),(9,14)],fill=(126,95,77,255),outline=(77,62,53,255)); d.rectangle((15,21,21,25),fill=(236,222,204,255)); d.rectangle((27,21,33,25),fill=(236,222,204,255)); d.rectangle((20,32,28,34),fill=(72,55,49,255))
    else: # generic file
        rounded(d,(9,5,39,43),4); d.polygon([(30,5),(39,14),(30,14)],fill=(210,216,224,255),outline=mid); d.line((14,21,34,21),fill=mid,width=2); d.line((14,27,34,27),fill=mid,width=2); d.line((14,33,28,33),fill=mid,width=2)
    return im

# Supplied source art becomes real ICO files too.
for name, source in [
    ('settings', ROOT/'ico'/'settings.png'),
    ('picture', ROOT/'ico'/'picture.png'),
    ('installer', ROOT/'ico'/'installer.png')
]:
    image = Image.open(source).convert('RGBA')
    image.thumbnail((42,42), Image.Resampling.LANCZOS)
    base = canvas(); base.alpha_composite(image, ((S-image.width)//2, (S-image.height)//2)); ico_write(base,name)

# All other shortcuts/apps use the generic file icon from ico/file.png
file_image = Image.open(ROOT/'ico'/'file.png').convert('RGBA')
file_image.thumbnail((42,42), Image.Resampling.LANCZOS)
file_base = canvas()
file_base.alpha_composite(file_image, ((S-file_image.width)//2, (S-file_image.height)//2))
for name in ['file', 'explorer', 'terminal', 'taskmgr', 'browser', 'notepad', 'paint', 'doom']:
    ico_write(file_base, name)

ico_write(simple('folder'), 'folder')
print(f'wrote {len(list(OUT.glob("*.ico")))} ICO files to {OUT}')
