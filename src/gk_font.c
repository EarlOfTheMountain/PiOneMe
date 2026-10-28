/* =========================================================================
 * gk_font.c — 8x8 bitmap font (public-domain font8x8 by Daniel Hepper /
 * Marcel Sondaar / IBM VGA font). Rendered through the sprite batch.
 * ========================================================================= */
#include "gk.h"
#include "gk_font.h"

#include "external/font8x8_basic.h"

#include <stdarg.h>

/* font8x8 rows are bytes, LSB = leftmost pixel column (verified by rendering) */
static bool gk__glyph_bit(char ch, int row, int col) {
    unsigned char b;
    unsigned char uch = (unsigned char)ch;
    if (uch >= 128) uch = '?';
    b = (unsigned char)font8x8_basic[uch][row];
    return (b >> col) & 1;
}

void gk_font_draw(gk_sprite* sb, const char* text, float x, float y,
                  float scale, gk_color col) {
    float cx = x, cy = y;
    const char* p;
    int r, c;

    if (!sb || !text) return;
    for (p = text; *p; p++) {
        if (*p == '\n') {
            cx = x;
            cy += 8.0f * scale;
            continue;
        }
        for (r = 0; r < 8; r++) {
            for (c = 0; c < 8; c++) {
                if (gk__glyph_bit(*p, r, c)) {
                    gk_sprite_rect(sb, cx + (float)c * scale,
                                   cy + (float)r * scale, scale, scale, col);
                }
            }
        }
        cx += 8.0f * scale;
    }
}

void gk_font_drawf(gk_sprite* sb, float x, float y, float scale,
                   gk_color col, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    gk_font_draw(sb, buf, x, y, scale, col);
}

float gk_font_width(const char* text, float scale) {
    float max_len = 0.0f, line_len = 0.0f;
    const char* p;
    if (!text) return 0.0f;
    for (p = text; *p; p++) {
        if (*p == '\n') {
            if (line_len > max_len) max_len = line_len;
            line_len = 0.0f;
        } else {
            line_len += 1.0f;
        }
    }
    if (line_len > max_len) max_len = line_len;
    return max_len * 8.0f * scale;
}

float gk_font_height(int lines, float scale) {
    if (lines < 1) lines = 1;
    return (float)lines * 8.0f * scale;
}
