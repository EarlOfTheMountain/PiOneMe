/* gk_texture.c — RGBA8 texture upload + image loading via stb_image */
#include "gk.h"
#include "gk_texture.h"
#include "gk_assets.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "external/stb_image.h"

#include <errno.h>

int gk_texture_create(gk_texture* t, const void* rgba, int w, int h,
                      gk_filter filter, gk_wrap wrap) {
    if (!t || !rgba || w <= 0 || h <= 0) return -1;
    memset(t, 0, sizeof *t);
    t->w = w;
    t->h = h;

    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    filter == GK_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    filter == GK_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    wrap == GK_WRAP_REPEAT ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    wrap == GK_WRAP_REPEAT ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    GK_CHECK_GL();
    return 0;
}

int gk_texture_load(gk_texture* t, const char* asset_path,
                    gk_filter filter, gk_wrap wrap) {
    const char* full = gk_asset_path(asset_path);
    int w = 0, h = 0, comp = 0;
    unsigned char* data;

    if (!t || !asset_path) return -1;
    memset(t, 0, sizeof *t);

    if (!gk_file_exists(full)) {
        GK_ERROR("texture not found: %s\n", full);
        return -1;
    }
    data = stbi_load(full, &w, &h, &comp, 4);
    if (!data) {
        GK_ERROR("failed to decode %s: %s\n", full, stbi_failure_reason());
        return -1;
    }
    {
        int rc = gk_texture_create(t, data, w, h, filter, wrap);
        stbi_image_free(data);
        if (rc == 0)
            GK_LOG("texture %s (%dx%d)\n", asset_path, w, h);
        return rc;
    }
}

void gk_texture_destroy(gk_texture* t) {
    if (!t || !t->id) return;
    glDeleteTextures(1, &t->id);
    memset(t, 0, sizeof *t);
}

void gk_texture_bind(const gk_texture* t, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, t ? t->id : gk_texture_white()->id);
}

const gk_texture* gk_texture_white(void) {
    static gk_texture white;
    static bool inited = false;
    if (!inited) {
        const uint32_t px = 0xFFFFFFFFu;
        if (gk_texture_create(&white, &px, 1, 1, GK_FILTER_NEAREST, GK_WRAP_CLAMP) != 0) {
            GK_ERROR("failed to create white texture\n");
        }
        inited = true;
    }
    return &white;
}
