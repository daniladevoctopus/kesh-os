import os
from PIL import Image, ImageFont, ImageDraw

font = ImageFont.truetype('GoogleSans-Medium.ttf', 12)

out = []
out.append('// Автосгенерированный сглаженный (grayscale, alpha-blend) шрифт Google Sans (Medium)')
out.append('// ПРОПОРЦИОНАЛЬНЫЙ: у каждого символа своя реальная ширина шага (advance),')
out.append('// растр хранится в ячейке 12x16, но курсор двигается на font_advance[], а не на фикс. число.')
out.append('#include "font.h"')
out.append('')
out.append('#define FONT_FIRST_CHAR 0x20')
out.append('#define FONT_LAST_CHAR  0x7E')
out.append('#define FONT_CHAR_W 12')
out.append('#define FONT_CHAR_H 16')
out.append('')
out.append('static const unsigned char font[95][FONT_CHAR_H][FONT_CHAR_W] = {')

advances = []

for c in range(32, 127):
    ch = chr(c)
    im = Image.new('L', (12, 16), 0)
    draw = ImageDraw.Draw(im)
    draw.text((0, 0), ch, font=font, fill=255)
    
    # Escape comment
    safe_ch = ch if ch not in ['\\', '"', '/*'] else hex(c)
    out.append(f'    {{ // \'{safe_ch}\' ({hex(c)})')
    for y in range(16):
        row_vals = [str(im.getpixel((x, y))).rjust(4) for x in range(12)]
        out.append('        {' + ','.join(row_vals) + ' },')
    out.append('    },')
    
    adv = int(round(font.getlength(ch)))
    if c == 32: adv = 4
    if adv < 2: adv = 2
    if adv > 12: adv = 12
    advances.append(adv)

out.append('};')
out.append('')
out.append('static const unsigned char font_advance[95] = {')
for i in range(0, len(advances), 16):
    chunk = advances[i:i+16]
    out.append('    ' + ', '.join(f'{a}' for a in chunk) + ',')
out.append('};')
out.append('')
out.append('''static inline uint32_t blend_pixel(uint32_t bg, uint32_t fg, unsigned char alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    unsigned int inv = 255 - alpha;

    unsigned int bg_r = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bg_b = bg & 0xFF;
    unsigned int fg_r = (fg >> 16) & 0xFF, fg_g = (fg >> 8) & 0xFF, fg_b = fg & 0xFF;

    unsigned int r = (fg_r * alpha + bg_r * inv) / 255;
    unsigned int g = (fg_g * alpha + bg_g * inv) / 255;
    unsigned int b = (fg_b * alpha + bg_b * inv) / 255;

    return (r << 16) | (g << 8) | b;
}

static unsigned char char_advance(unsigned char code) {
    if (code < FONT_FIRST_CHAR || code > FONT_LAST_CHAR) {
        code = '?';
    }
    return font_advance[code - FONT_FIRST_CHAR];
}

void draw_char(char c, int x, int y, uint32_t color, uint32_t* buf, uint32_t buf_width) {
    unsigned char code = (unsigned char)c;
    if (code < FONT_FIRST_CHAR || code > FONT_LAST_CHAR) {
        code = '?';
    }
    const unsigned char (*glyph)[FONT_CHAR_W] = font[code - FONT_FIRST_CHAR];

    for (int row = 0; row < FONT_CHAR_H; row++) {
        for (int col = 0; col < FONT_CHAR_W; col++) {
            unsigned char alpha = glyph[row][col];
            if (alpha == 0) continue;

            uint32_t* dst = &buf[(y + row) * buf_width + (x + col)];
            *dst = blend_pixel(*dst, color, alpha);
        }
    }
}

void draw_string(const char* str, int x, int y, uint32_t color, uint32_t* buf, uint32_t buf_width) {
    int cursor_x = x;
    while (*str) {
        if (*str == '\\n') {
            cursor_x = x;
            y += FONT_CHAR_H;
        } else {
            draw_char(*str, cursor_x, y, color, buf, buf_width);
            cursor_x += char_advance((unsigned char)*str);
        }
        str++;
    }
}

int font_text_width(const char* str) {
    int w = 0;
    while (*str) {
        w += char_advance((unsigned char)*str);
        str++;
    }
    return w;
}
''')

with open('src/gui/font.c', 'w', encoding='utf-8') as f:
    f.write('\n'.join(out))

print('Wrote src/gui/font.c successfully, size:', len('\n'.join(out)))
