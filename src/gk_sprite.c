/* =========================================================================
 * gk_sprite.c — batched textured quads (one draw call per texture change)
 * ========================================================================= */
#include "gk.h"
#include "gk_sprite.h"
#include "gk_shader.h"
#include "gk_assets.h"

#include <GLES2/gl2.h>

typedef struct {
    float x, y, u, v;
    gk_u8 r, g, b, a;
} gk__svert;   /* 20 bytes */

struct gk_sprite {
    gk_shader* sh;          /* may be loaded from assets, may be embedded */
    bool sh_owned;          /* embedded fallback must be freed with the batch */
    GLuint vbo, ibo;
    int max_quads;
    int count;
    gk_mat4 mvp;
    const gk_texture* cur_tex;
    gk__svert* verts;
};

/* Embedded fallback (used when assets/shaders/sprite.{vert,frag} are absent) */
static const char* GK_SPRITE_VS =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_col;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    v_col = a_col;\n"
    "    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* GK_SPRITE_FS =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_tex, v_uv) * v_col;\n"
    "}\n";

static void gk__sprite_flush(gk_sprite* sb) {
    GLint a_pos, a_uv, a_col;
    if (!sb || sb->count == 0) return;

    gk_shader_use(sb->sh);
    gk_shader_set_mat4(sb->sh, "u_mvp", &sb->mvp);
    gk_shader_set_int(sb->sh, "u_tex", 0);
    gk_texture_bind(sb->cur_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, sb->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)sb->max_quads * 4 * (GLsizeiptr)sizeof(gk__svert),
                 NULL, GL_STREAM_DRAW);   /* orphan */
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)sb->count * 4 * (GLsizeiptr)sizeof(gk__svert),
                    sb->verts);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sb->ibo);

    a_pos = gk_shader_attrib(sb->sh, "a_pos");
    a_uv  = gk_shader_attrib(sb->sh, "a_uv");
    a_col = gk_shader_attrib(sb->sh, "a_col");
    if (a_pos >= 0) {
        glEnableVertexAttribArray((GLuint)a_pos);
        glVertexAttribPointer((GLuint)a_pos, 2, GL_FLOAT, GL_FALSE,
                              sizeof(gk__svert), (const void*)0);
    }
    if (a_uv >= 0) {
        glEnableVertexAttribArray((GLuint)a_uv);
        glVertexAttribPointer((GLuint)a_uv, 2, GL_FLOAT, GL_FALSE,
                              sizeof(gk__svert), (const void*)8);
    }
    if (a_col >= 0) {
        glEnableVertexAttribArray((GLuint)a_col);
        glVertexAttribPointer((GLuint)a_col, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                              sizeof(gk__svert), (const void*)16);
    }

    glDrawElements(GL_TRIANGLES, sb->count * 6, GL_UNSIGNED_SHORT, 0);

    if (a_pos >= 0) glDisableVertexAttribArray((GLuint)a_pos);
    if (a_uv >= 0)  glDisableVertexAttribArray((GLuint)a_uv);
    if (a_col >= 0) glDisableVertexAttribArray((GLuint)a_col);

    sb->count = 0;
}

gk_sprite* gk_sprite_create(int max_quads) {
    gk_sprite* sb;
    gk_u16* idx_tmp;
    int i;

    if (max_quads < 16) max_quads = 16;
    if (max_quads > 65536 / 4) max_quads = 65536 / 4;

    sb = (gk_sprite*)calloc(1, sizeof(gk_sprite));
    if (!sb) return NULL;
    sb->max_quads = max_quads;
    sb->verts = (gk__svert*)malloc(sizeof(gk__svert) * (size_t)max_quads * 4);
    idx_tmp = (gk_u16*)malloc(sizeof(gk_u16) * (size_t)max_quads * 6);
    if (!sb->verts || !idx_tmp) {
        free(sb->verts);
        free(idx_tmp);
        free(sb);
        return NULL;
    }
    for (i = 0; i < max_quads; i++) {
        gk_u16 b = (gk_u16)(i * 4);
        gk_u16* q = &idx_tmp[i * 6];
        q[0] = b;     q[1] = (gk_u16)(b + 1); q[2] = (gk_u16)(b + 2);
        q[3] = b;     q[4] = (gk_u16)(b + 2); q[5] = (gk_u16)(b + 3);
    }

    glGenBuffers(1, &sb->vbo);
    glGenBuffers(1, &sb->ibo);
    glBindBuffer(GL_ARRAY_BUFFER, sb->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)max_quads * 4 * (GLsizeiptr)sizeof(gk__svert),
                 NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sb->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)max_quads * 6 * (GLsizeiptr)sizeof(gk_u16),
                 idx_tmp, GL_STATIC_DRAW);
    free(idx_tmp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    sb->sh = gk_shader_load("shaders/sprite");
    if (sb->sh) {
        sb->sh_owned = false;
        GK_LOG("sprite batch: using assets/shaders/sprite\n");
    } else {
        sb->sh = gk_shader_create(GK_SPRITE_VS, GK_SPRITE_FS);
        sb->sh_owned = true;
        GK_LOG("sprite batch: using embedded shader\n");
    }
    if (!sb->sh) {
        free(sb->verts);
        free(sb);
        return NULL;
    }
    sb->count = 0;
    sb->cur_tex = NULL;
    return sb;
}

void gk_sprite_destroy(gk_sprite* sb) {
    if (!sb) return;
    if (sb->sh_owned) gk_shader_destroy(sb->sh);
    glDeleteBuffers(1, &sb->vbo);
    glDeleteBuffers(1, &sb->ibo);
    free(sb->verts);
    free(sb);
}

void gk_sprite_begin(gk_sprite* sb, const gk_mat4* mvp) {
    if (!sb) return;
    gk__sprite_flush(sb);
    sb->mvp = *mvp;
    sb->count = 0;
    sb->cur_tex = NULL;
}

void gk_sprite_begin_screen(gk_sprite* sb, int screen_w, int screen_h) {
    gk_mat4 mvp = gk_mat4_ortho_topleft((float)screen_w, (float)screen_h);
    gk_sprite_begin(sb, &mvp);
}

void gk_sprite_draw(gk_sprite* sb, const gk_texture* tex,
                    float x, float y, float w, float h,
                    float sx, float sy, float sw, float sh,
                    float rot, float origin_x, float origin_y,
                    gk_color tint, int flip) {
    const float tw = tex ? (float)tex->w : 1.0f;
    const float th = tex ? (float)tex->h : 1.0f;
    float u0 = sx / tw,        v0 = sy / th;
    float u1 = (sx + sw) / tw, v1 = (sy + sh) / th;
    float c = cosf(rot), s = sinf(rot);
    float lx[4], ly[4], uu[4], vv[4];
    int i;
    gk__svert* out;

    if (!sb || !tex || w <= 0.0f || h <= 0.0f) return;
    if (sb->count >= sb->max_quads)
        gk__sprite_flush(sb);

    if (flip & GK_FLIP_H) { float t = u0; u0 = u1; u1 = t; }
    if (flip & GK_FLIP_V) { float t = v0; v0 = v1; v1 = t; }

    /* corners in sprite-local space, relative to the pivot */
    lx[0] = -origin_x;       ly[0] = -origin_y;       uu[0] = u0; vv[0] = v0;
    lx[1] = w - origin_x;    ly[1] = -origin_y;       uu[1] = u1; vv[1] = v0;
    lx[2] = w - origin_x;    ly[2] = h - origin_y;    uu[2] = u1; vv[2] = v1;
    lx[3] = -origin_x;       ly[3] = h - origin_y;    uu[3] = u0; vv[3] = v1;

    if (sb->cur_tex != tex) {
        gk__sprite_flush(sb);
        sb->cur_tex = tex;
    }

    out = &sb->verts[sb->count * 4];
    for (i = 0; i < 4; i++) {
        out[i].x = x + lx[i] * c - ly[i] * s;
        out[i].y = y + lx[i] * s + ly[i] * c;
        out[i].u = uu[i];
        out[i].v = vv[i];
        out[i].r = (gk_u8)(gk_clampf(tint.r, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i].g = (gk_u8)(gk_clampf(tint.g, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i].b = (gk_u8)(gk_clampf(tint.b, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i].a = (gk_u8)(gk_clampf(tint.a, 0.0f, 1.0f) * 255.0f + 0.5f);
    }
    sb->count++;
}

void gk_sprite_rect(gk_sprite* sb, float x, float y, float w, float h, gk_color c) {
    gk_sprite_draw(sb, gk_texture_white(), x, y, w, h,
                   0.0f, 0.0f, 1.0f, 1.0f,
                   0.0f, 0.0f, 0.0f, c, GK_FLIP_NONE);
}

void gk_sprite_end(gk_sprite* sb) {
    gk__sprite_flush(sb);
}

int gk_sprite_quad_count(const gk_sprite* sb) {
    return sb ? sb->count : 0;
}
