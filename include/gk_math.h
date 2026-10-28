/* =========================================================================
 * gk_math.h — vectors, colors, 4x4 matrices, camera
 *
 * Matrices are column-major: element (row, col) lives at m[col * 4 + row].
 * That matches glUniformMatrix4fv(..., GL_FALSE, m) directly.
 *
 * 2D convention: +x right, +y DOWN (screen coordinates), so the default
 * 2D projection is gk_mat4_ortho_topleft().
 * 3D convention: right-handed, +y up, camera looks down -z by default.
 * ========================================================================= */
#ifndef GK_MATH_H
#define GK_MATH_H

#include "gk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- vectors ----------------------------------------------------------- */
typedef struct { float x, y; }       gk_vec2;
typedef struct { float x, y, z; }    gk_vec3;
typedef struct { float x, y, z, w; } gk_vec4;
typedef struct { int x, y; }         gk_ivec2;
typedef struct { float x, y, w, h; } gk_rect;   /* x,y = top-left in 2D */

static inline gk_vec2 gk_v2(float x, float y) { gk_vec2 v; v.x = x; v.y = y; return v; }
static inline gk_vec3 gk_v3(float x, float y, float z) { gk_vec3 v; v.x = x; v.y = y; v.z = z; return v; }

static inline gk_vec2 gk_vec2_add(gk_vec2 a, gk_vec2 b) { return gk_v2(a.x + b.x, a.y + b.y); }
static inline gk_vec2 gk_vec2_sub(gk_vec2 a, gk_vec2 b) { return gk_v2(a.x - b.x, a.y - b.y); }
static inline gk_vec2 gk_vec2_scale(gk_vec2 a, float s) { return gk_v2(a.x * s, a.y * s); }
static inline float   gk_vec2_dot(gk_vec2 a, gk_vec2 b) { return a.x * b.x + a.y * b.y; }
static inline float   gk_vec2_len(gk_vec2 a) { return sqrtf(a.x * a.x + a.y * a.y); }

static inline gk_vec3 gk_vec3_add(gk_vec3 a, gk_vec3 b) { return gk_v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline gk_vec3 gk_vec3_sub(gk_vec3 a, gk_vec3 b) { return gk_v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline gk_vec3 gk_vec3_scale(gk_vec3 a, float s) { return gk_v3(a.x * s, a.y * s, a.z * s); }
static inline float   gk_vec3_dot(gk_vec3 a, gk_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float   gk_vec3_len(gk_vec3 a) { return sqrtf(gk_vec3_dot(a, a)); }
static inline gk_vec3 gk_vec3_cross(gk_vec3 a, gk_vec3 b) {
    return gk_v3(a.y * b.z - a.z * b.y,
                   a.z * b.x - a.x * b.z,
                   a.x * b.y - a.y * b.x);
}
static inline gk_vec3 gk_vec3_norm(gk_vec3 a) {
    float l = gk_vec3_len(a);
    return l > 1e-8f ? gk_vec3_scale(a, 1.0f / l) : a;
}
static inline gk_vec3 gk_vec3_lerp(gk_vec3 a, gk_vec3 b, float t) {
    return gk_v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

/* ---- colors (floats 0..1) ---------------------------------------------- */
typedef struct { float r, g, b, a; } gk_color;

static inline gk_color gk_col(float r, float g, float b, float a) {
    gk_color c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
}
static inline gk_color gk_col8(int r, int g, int b, int a) {
    return gk_col(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}
/* 0xRRGGBBAA */
static inline gk_color gk_col_hex(uint32_t rgba) {
    return gk_col8((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF);
}
static inline gk_color gk_col_lerp(gk_color a, gk_color b, float t) {
    return gk_col(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                  a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

#define GK_WHITE  gk_col(1.0f, 1.0f, 1.0f, 1.0f)
#define GK_BLACK  gk_col(0.0f, 0.0f, 0.0f, 1.0f)
#define GK_RED    gk_col(0.90f, 0.20f, 0.15f, 1.0f)
#define GK_GREEN  gk_col(0.25f, 0.80f, 0.30f, 1.0f)
#define GK_BLUE   gk_col(0.25f, 0.45f, 0.95f, 1.0f)
#define GK_YELLOW gk_col(0.95f, 0.85f, 0.25f, 1.0f)
#define GK_ORANGE gk_col(0.95f, 0.55f, 0.15f, 1.0f)

/* ---- mat4 --------------------------------------------------------------- */
typedef struct { float m[16]; } gk_mat4;

gk_mat4 gk_mat4_identity(void);
/* result = a * b  (b is applied first) */
gk_mat4 gk_mat4_mul(gk_mat4 a, gk_mat4 b);
/* transforms a point (w = 1), no perspective divide */
gk_vec3 gk_mat4_mul_point(gk_mat4 m, gk_vec3 p);

gk_mat4 gk_mat4_ortho(float l, float r, float b, float t, float n, float f);
/* 2D projection: (0,0) top-left, +y down, depth 0..1 -> use z = 0 */
gk_mat4 gk_mat4_ortho_topleft(float w, float h);
gk_mat4 gk_mat4_perspective(float fov_y_deg, float aspect, float zn, float zf);
gk_mat4 gk_mat4_look_at(gk_vec3 eye, gk_vec3 at, gk_vec3 up);

gk_mat4 gk_mat4_translate(float x, float y, float z);
gk_mat4 gk_mat4_scale(float x, float y, float z);
gk_mat4 gk_mat4_rot_x(float rad);
gk_mat4 gk_mat4_rot_y(float rad);
gk_mat4 gk_mat4_rot_z(float rad);
/* translate * rot_z * scale — handy for 2D */
gk_mat4 gk_mat4_trs(gk_vec3 t, float rot_z_rad, gk_vec3 s);

/* ---- camera -------------------------------------------------------------- */
typedef struct gk_camera {
    gk_vec3 pos;
    gk_vec3 target;
    gk_vec3 up;
    float fov_y;    /* vertical fov, degrees */
    float near_z;
    float far_z;
} gk_camera;

gk_mat4 gk_camera_view(const gk_camera* cam);
gk_mat4 gk_camera_proj(const gk_camera* cam, float aspect);

#ifdef __cplusplus
}
#endif

#endif /* GK_MATH_H */
