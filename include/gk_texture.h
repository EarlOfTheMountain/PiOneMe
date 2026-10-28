/* =========================================================================
 * gk_texture.h — RGBA8 textures (PNG/JPEG/BMP/TGA via stb_image)
 *
 * UV convention for the whole framework: v = 0 is the TOP row of the image
 * (y-down, like screen coordinates). NPOT sizes are fine (clamp wrap only
 * is required by GLES2 for NPOT when using repeat — repeat needs POT).
 * ========================================================================= */
#ifndef GK_TEXTURE_H
#define GK_TEXTURE_H

#include "gk.h"
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { GK_FILTER_LINEAR = 0, GK_FILTER_NEAREST } gk_filter;
typedef enum { GK_WRAP_CLAMP = 0, GK_WRAP_REPEAT = 1 }   gk_wrap;

typedef struct gk_texture {
    GLuint id;
    int w, h;
} gk_texture;

/* Create from raw RGBA8 pixel data (row 0 = top). Returns 0 on success. */
int  gk_texture_create(gk_texture* t, const void* rgba, int w, int h,
                       gk_filter filter, gk_wrap wrap);

/* Load from an asset-relative path, e.g. "images/road.png" */
int  gk_texture_load(gk_texture* t, const char* asset_path,
                     gk_filter filter, gk_wrap wrap);

void gk_texture_destroy(gk_texture* t);
void gk_texture_bind(const gk_texture* t, int unit);

/* 1x1 white fallback texture (used for solid quads / fonts) */
const gk_texture* gk_texture_white(void);

#ifdef __cplusplus
}
#endif

#endif /* GK_TEXTURE_H */
