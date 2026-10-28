/* gk_math.c — matrices and camera */
#include "gk_math.h"

gk_mat4 gk_mat4_identity(void) {
    gk_mat4 r;
    memset(&r, 0, sizeof r);
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

/* column-major: m[col * 4 + row] */
gk_mat4 gk_mat4_mul(gk_mat4 a, gk_mat4 b) {
    gk_mat4 r;
    int c, row, k;
    for (c = 0; c < 4; c++) {
        for (row = 0; row < 4; row++) {
            float s = 0.0f;
            for (k = 0; k < 4; k++)
                s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    }
    return r;
}

gk_vec3 gk_mat4_mul_point(gk_mat4 m, gk_vec3 p) {
    gk_vec3 o;
    o.x = m.m[0] * p.x + m.m[4] * p.y + m.m[8]  * p.z + m.m[12];
    o.y = m.m[1] * p.x + m.m[5] * p.y + m.m[9]  * p.z + m.m[13];
    o.z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    return o;
}

gk_mat4 gk_mat4_ortho(float l, float r, float b, float t, float n, float f) {
    gk_mat4 m = gk_mat4_identity();
    float rl = r - l, tb = t - b, fn = f - n;
    if (rl == 0.0f || tb == 0.0f || fn == 0.0f) return m;
    m.m[0]  = 2.0f / rl;
    m.m[5]  = 2.0f / tb;
    m.m[10] = -2.0f / fn;
    m.m[12] = -(r + l) / rl;
    m.m[13] = -(t + b) / tb;
    m.m[14] = -(f + n) / fn;
    return m;
}

gk_mat4 gk_mat4_ortho_topleft(float w, float h) {
    /* x: 0..w -> -1..1 ; y: 0 (top) -> -1, h (bottom) -> 1 */
    return gk_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
}

gk_mat4 gk_mat4_perspective(float fov_y_deg, float aspect, float zn, float zf) {
    gk_mat4 r;
    float f, fn;
    memset(&r, 0, sizeof r);
    if (zn == zf || aspect == 0.0f) return gk_mat4_identity();
    f = 1.0f / tanf(gk_radf(fov_y_deg) * 0.5f);
    fn = 1.0f / (zn - zf);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zf + zn) * fn;
    r.m[11] = -1.0f;
    r.m[14] = 2.0f * zf * zn * fn;
    return r;
}

gk_mat4 gk_mat4_look_at(gk_vec3 eye, gk_vec3 at, gk_vec3 up) {
    gk_mat4 r;
    gk_vec3 fwd = gk_vec3_norm(gk_vec3_sub(at, eye));   /* camera looks along +fwd */
    gk_vec3 s   = gk_vec3_norm(gk_vec3_cross(fwd, up));
    gk_vec3 u   = gk_vec3_cross(s, fwd);
    memset(&r, 0, sizeof r);
    r.m[0] = s.x;   r.m[4] = s.y;   r.m[8]  = s.z;
    r.m[1] = u.x;   r.m[5] = u.y;   r.m[9]  = u.z;
    r.m[2] = -fwd.x;r.m[6] = -fwd.y;r.m[10] = -fwd.z;
    r.m[12] = -gk_vec3_dot(s, eye);
    r.m[13] = -gk_vec3_dot(u, eye);
    r.m[14] =  gk_vec3_dot(fwd, eye);
    r.m[15] = 1.0f;
    return r;
}

gk_mat4 gk_mat4_translate(float x, float y, float z) {
    gk_mat4 r = gk_mat4_identity();
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}

gk_mat4 gk_mat4_scale(float x, float y, float z) {
    gk_mat4 r = gk_mat4_identity();
    r.m[0] = x; r.m[5] = y; r.m[10] = z;
    return r;
}

gk_mat4 gk_mat4_rot_x(float rad) {
    gk_mat4 r = gk_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    r.m[5]  = c;  r.m[9]  = -s;
    r.m[6]  = s;  r.m[10] =  c;
    return r;
}

gk_mat4 gk_mat4_rot_y(float rad) {
    gk_mat4 r = gk_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    r.m[0]  =  c;  r.m[8]  = s;
    r.m[2]  = -s;  r.m[10] = c;
    return r;
}

gk_mat4 gk_mat4_rot_z(float rad) {
    gk_mat4 r = gk_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    r.m[0] = c;  r.m[4] = -s;
    r.m[1] = s;  r.m[5] =  c;
    return r;
}

gk_mat4 gk_mat4_trs(gk_vec3 t, float rot_z_rad, gk_vec3 s) {
    gk_mat4 m = gk_mat4_mul(gk_mat4_translate(t.x, t.y, t.z),
                            gk_mat4_rot_z(rot_z_rad));
    return gk_mat4_mul(m, gk_mat4_scale(s.x, s.y, s.z));
}

gk_mat4 gk_camera_view(const gk_camera* cam) {
    return gk_mat4_look_at(cam->pos, cam->target, cam->up);
}

gk_mat4 gk_camera_proj(const gk_camera* cam, float aspect) {
    return gk_mat4_perspective(cam->fov_y, aspect, cam->near_z, cam->far_z);
}
