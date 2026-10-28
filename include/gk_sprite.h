/* =========================================================================
 * gk_sprite.h — batched 2D quad renderer (HUD, particles, UI)
 *
 * One draw call per texture change. Owns its internal shader: it is loaded
 * from "shaders/sprite" when present, otherwise an embedded default is used.
 *
 * Sprite shader contract (custom shaders must match):
 *   attributes: vec2 a_pos, vec2 a_uv, vec4 a_col (normalized bytes)
 *   uniforms  : mat4 u_mvp, sampler2D u_tex
 * ========================================================================= */
#ifndef GK_SPRITE_H
#define GK_SPRITE_H

#include "gk.h"
#include "gk_math.h"
#include "gk_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gk_sprite gk_sprite;

enum { GK_FLIP_NONE = 0, GK_FLIP_H = 1, GK_FLIP_V = 2 };

gk_sprite* gk_sprite_create(int max_quads);
void       gk_sprite_destroy(gk_sprite* sb);

/* Begin a batch with an explicit projection. */
void gk_sprite_begin(gk_sprite* sb, const gk_mat4* mvp);
/* Begin a batch in screen pixels: (0,0) top-left, y down. */
void gk_sprite_begin_screen(gk_sprite* sb, int screen_w, int screen_h);

/* Draw one textured quad.
 *   x,y, w,h     destination rectangle (top-left corner before rotation)
 *   sx,sy, sw,sh source rect in texture pixels (0,0 = top-left of image)
 *   rot          rotation in radians around the pivot
 *   ox, oy       pivot inside the sprite, in pixels (e.g. center = w/2,h/2)
 *   tint         multiplier color, GK_WHITE = unmodified
 *   flip         GK_FLIP_H / GK_FLIP_V bitmask
 */
void gk_sprite_draw(gk_sprite* sb, const gk_texture* tex,
                    float x, float y, float w, float h,
                    float sx, float sy, float sw, float sh,
                    float rot, float origin_x, float origin_y,
                    gk_color tint, int flip);

/* Untextured solid rectangle (uses the internal 1x1 white texture). */
void gk_sprite_rect(gk_sprite* sb, float x, float y, float w, float h, gk_color c);

/* Flushes everything to the GPU. Call once per frame after drawing. */
void gk_sprite_end(gk_sprite* sb);

int gk_sprite_quad_count(const gk_sprite* sb);

#ifdef __cplusplus
}
#endif

#endif /* GK_SPRITE_H */
