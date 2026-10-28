/* =========================================================================
 * gk_font.h — built-in 8x8 bitmap font (public-domain font8x8)
 *
 * Zero-dependency text for HUDs and debug output. Draw through a sprite
 * batch. Monospace: each glyph is 8x8 pixels at scale 1.
 * ========================================================================= */
#ifndef GK_FONT_H
#define GK_FONT_H

#include "gk.h"
#include "gk_math.h"
#include "gk_sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Draws text at the TOP-LEFT of the first glyph. Handles '\n'. */
void  gk_font_draw(gk_sprite* sb, const char* text, float x, float y,
                   float scale, gk_color col);

/* printf-style wrapper (256 byte buffer). */
void  gk_font_drawf(gk_sprite* sb, float x, float y, float scale,
                    gk_color col, const char* fmt, ...);

/* Width of the widest line, in pixels. */
float gk_font_width(const char* text, float scale);
/* Height of the text block (lines * 8 * scale). */
float gk_font_height(int lines, float scale);

#ifdef __cplusplus
}
#endif

#endif /* GK_FONT_H */
