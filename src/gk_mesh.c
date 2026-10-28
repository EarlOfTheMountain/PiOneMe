/* =========================================================================
 * gk_mesh.c — VBO/IBO meshes: primitives, OBJ loading, attribute binding
 * ========================================================================= */
#include "gk.h"
#include "gk_mesh.h"
#include "gk_assets.h"

#include <stdio.h>

int gk_mesh_create(gk_mesh* m, const gk_mesh_vertex* verts, int nv,
                   const gk_u16* indices, int ni) {
    if (!m || !verts || !indices || nv <= 0 || ni <= 0 || nv > 65536) {
        GK_ERROR("gk_mesh_create: bad args (nv=%d ni=%d)\n", nv, ni);
        return -1;
    }
    memset(m, 0, sizeof *m);
    glGenBuffers(1, &m->vbo);
    glGenBuffers(1, &m->ibo);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)nv * sizeof(gk_mesh_vertex),
                 verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ni * sizeof(gk_u16),
                 indices, GL_STATIC_DRAW);
    m->index_count = ni;
    GK_CHECK_GL();
    return 0;
}

void gk_mesh_destroy(gk_mesh* m) {
    if (!m) return;
    if (m->vbo) glDeleteBuffers(1, &m->vbo);
    if (m->ibo) glDeleteBuffers(1, &m->ibo);
    memset(m, 0, sizeof *m);
}

void gk_mesh_draw(const gk_mesh* m, const gk_shader* sh) {
    GLint a_pos, a_nrm, a_uv;
    if (!m || !sh || m->index_count <= 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);

    a_pos = gk_shader_attrib(sh, "a_pos");
    if (a_pos >= 0) {
        glEnableVertexAttribArray((GLuint)a_pos);
        glVertexAttribPointer((GLuint)a_pos, 3, GL_FLOAT, GL_FALSE,
                              sizeof(gk_mesh_vertex), (const void*)0);
    }
    a_nrm = gk_shader_attrib(sh, "a_nrm");
    if (a_nrm >= 0) {
        glEnableVertexAttribArray((GLuint)a_nrm);
        glVertexAttribPointer((GLuint)a_nrm, 3, GL_FLOAT, GL_FALSE,
                              sizeof(gk_mesh_vertex), (const void*)12);
    }
    a_uv = gk_shader_attrib(sh, "a_uv");
    if (a_uv >= 0) {
        glEnableVertexAttribArray((GLuint)a_uv);
        glVertexAttribPointer((GLuint)a_uv, 2, GL_FLOAT, GL_FALSE,
                              sizeof(gk_mesh_vertex), (const void*)24);
    }

    glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_SHORT, 0);

    if (a_pos >= 0) glDisableVertexAttribArray((GLuint)a_pos);
    if (a_nrm >= 0) glDisableVertexAttribArray((GLuint)a_nrm);
    if (a_uv >= 0) glDisableVertexAttribArray((GLuint)a_uv);
}

/* ---- helpers ------------------------------------------------------------ */

/* Build a face quad: origin + u + v with explicit normal.
   Corners: o, o+u, o+u+v, o+v ; uv corners: (u0,v0) (u1,v0) (u1,v1) (u0,v1) */
static void gk__face(gk_mesh_vertex* v, gk_u16* idx, int* nv, int* ni,
                     float ox, float oy, float oz,
                     float ux, float uy, float uz,
                     float vx, float vy, float vz,
                     float nx, float ny, float nz,
                     float uv_tile) {
    static const int quad[6] = { 0, 1, 2, 0, 2, 3 };
    float cs[4][3] = {
        { ox,           oy,           oz           },
        { ox + ux,      oy + uy,      oz + uz      },
        { ox + ux + vx, oy + uy + vy, oz + uz + vz },
        { ox + vx,      oy + vy,      oz + vz      }
    };
    float uvs[4][2] = {
        { 0.0f,     0.0f     },
        { uv_tile,  0.0f     },
        { uv_tile,  uv_tile  },
        { 0.0f,     uv_tile  }
    };
    int i, base = *nv;
    for (i = 0; i < 4; i++) {
        v[base + i].pos[0] = cs[i][0];
        v[base + i].pos[1] = cs[i][1];
        v[base + i].pos[2] = cs[i][2];
        v[base + i].nrm[0] = nx;
        v[base + i].nrm[1] = ny;
        v[base + i].nrm[2] = nz;
        v[base + i].uv[0]  = uvs[i][0];
        v[base + i].uv[1]  = uvs[i][1];
    }
    for (i = 0; i < 6; i++)
        idx[*ni + i] = (gk_u16)(base + quad[i]);
    *nv += 4;
    *ni += 6;
}

int gk_mesh_cube(gk_mesh* m, float size, float uv_tile) {
    static gk_mesh_vertex verts[24];
    static gk_u16 idx[36];
    int nv = 0, ni = 0;
    float h = size * 0.5f;

    if (size <= 0.0f) return -1;

    gk__face(verts, idx, &nv, &ni,  h, -h, -h,  0, 2*h, 0,   0, 0, 2*h,   1, 0, 0, uv_tile); /* +X */
    gk__face(verts, idx, &nv, &ni, -h, -h,  h,  0, 2*h, 0,   0, 0,-2*h,  -1, 0, 0, uv_tile); /* -X */
    gk__face(verts, idx, &nv, &ni, -h,  h, -h,  0, 0, 2*h,   2*h, 0, 0,    0, 1, 0, uv_tile); /* +Y */
    gk__face(verts, idx, &nv, &ni, -h, -h,  h,  0, 0,-2*h,   2*h, 0, 0,    0,-1, 0, uv_tile); /* -Y */
    gk__face(verts, idx, &nv, &ni, -h, -h,  h,  2*h, 0, 0,   0, 2*h, 0,    0, 0, 1, uv_tile); /* +Z */
    gk__face(verts, idx, &nv, &ni,  h, -h, -h, -2*h, 0, 0,   0, 2*h, 0,    0, 0,-1, uv_tile); /* -Z */

    return gk_mesh_create(m, verts, nv, idx, ni);
}

int gk_mesh_plane(gk_mesh* m, float w, float d,
                  int seg_w, int seg_d, float u_tile, float v_tile) {
    gk_mesh_vertex* verts;
    gk_u16* idx;
    int nv = 0, ni = 0, j, i;
    int rc;

    if (w <= 0.0f || d <= 0.0f) return -1;
    seg_w = seg_w < 1 ? 1 : seg_w;
    seg_d = seg_d < 1 ? 1 : seg_d;
    if ((seg_w + 1) * (seg_d + 1) > 65536) { GK_ERROR("plane: too many verts\n"); return -1; }

    verts = (gk_mesh_vertex*)malloc(sizeof(gk_mesh_vertex) * (size_t)((seg_w + 1) * (seg_d + 1)));
    idx   = (gk_u16*)malloc(sizeof(gk_u16) * (size_t)(seg_w * seg_d * 6));
    if (!verts || !idx) { free(verts); free(idx); return -1; }

    for (j = 0; j <= seg_d; j++) {
        for (i = 0; i <= seg_w; i++) {
            float x = -w * 0.5f + w * (float)i / (float)seg_w;
            float z = -d * 0.5f + d * (float)j / (float)seg_d;
            gk_mesh_vertex* vt = &verts[nv++];
            vt->pos[0] = x; vt->pos[1] = 0.0f; vt->pos[2] = z;
            vt->nrm[0] = 0.0f; vt->nrm[1] = 1.0f; vt->nrm[2] = 0.0f;
            vt->uv[0] = u_tile * (float)i / (float)seg_w;
            vt->uv[1] = v_tile * (float)j / (float)seg_d;
        }
    }
    for (j = 0; j < seg_d; j++) {
        for (i = 0; i < seg_w; i++) {
            int a = j * (seg_w + 1) + i;
            int b = a + seg_w + 1;
            idx[ni++] = (gk_u16)a;
            idx[ni++] = (gk_u16)b;
            idx[ni++] = (gk_u16)(a + 1);
            idx[ni++] = (gk_u16)(a + 1);
            idx[ni++] = (gk_u16)b;
            idx[ni++] = (gk_u16)(b + 1);
        }
    }
    rc = gk_mesh_create(m, verts, nv, idx, ni);
    free(verts);
    free(idx);
    return rc;
}

int gk_mesh_sphere(gk_mesh* m, float radius, int stacks, int slices) {
    gk_mesh_vertex* verts;
    gk_u16* idx;
    int nv = 0, ni = 0, i, j, rc;

    if (radius <= 0.0f) return -1;
    stacks = stacks < 2 ? 2 : stacks;
    slices = slices < 3 ? 3 : slices;

    verts = (gk_mesh_vertex*)malloc(sizeof(gk_mesh_vertex) * (size_t)((stacks + 1) * (slices + 1)));
    idx   = (gk_u16*)malloc(sizeof(gk_u16) * (size_t)stacks * slices * 6);
    if (!verts || !idx) { free(verts); free(idx); return -1; }

    for (i = 0; i <= stacks; i++) {
        float theta = (float)M_PI * (float)i / (float)stacks;   /* 0 = top pole */
        float y = cosf(theta);
        float r = sinf(theta);
        for (j = 0; j <= slices; j++) {
            float phi = 2.0f * (float)M_PI * (float)j / (float)slices;
            gk_mesh_vertex* vt = &verts[nv++];
            vt->pos[0] = radius * r * sinf(phi);
            vt->pos[1] = radius * y;
            vt->pos[2] = radius * r * cosf(phi);
            vt->nrm[0] = r * sinf(phi);
            vt->nrm[1] = y;
            vt->nrm[2] = r * cosf(phi);
            vt->uv[0] = (float)j / (float)slices;
            vt->uv[1] = (float)i / (float)stacks;    /* v = 0 at top pole */
        }
    }
    for (i = 0; i < stacks; i++) {
        for (j = 0; j < slices; j++) {
            int a = i * (slices + 1) + j;
            int b = (i + 1) * (slices + 1) + j;
            idx[ni++] = (gk_u16)a;
            idx[ni++] = (gk_u16)b;
            idx[ni++] = (gk_u16)(a + 1);
            idx[ni++] = (gk_u16)(a + 1);
            idx[ni++] = (gk_u16)b;
            idx[ni++] = (gk_u16)(b + 1);
        }
    }
    rc = gk_mesh_create(m, verts, nv, idx, ni);
    free(verts);
    free(idx);
    return rc;
}

/* ---- OBJ loader ----------------------------------------------------------- */

typedef struct { float x, y, z; } gk__f3;

typedef struct {
    gk__f3* data;
    int n, cap;
} gk__f3arr;

static bool gk__f3_push(gk__f3arr* a, float x, float y, float z) {
    if (a->n == a->cap) {
        int ncap = a->cap ? a->cap * 2 : 64;
        gk__f3* nd = (gk__f3*)realloc(a->data, sizeof(gk__f3) * (size_t)ncap);
        if (!nd) return false;
        a->data = nd;
        a->cap = ncap;
    }
    a->data[a->n].x = x;
    a->data[a->n].y = y;
    a->data[a->n].z = z;
    a->n++;
    return true;
}

static int gk__corner(const char* tok, int nv, int nvt, int nvn,
                      int* out_v, int* out_vt, int* out_vn) {
    /* "v", "v/vt", "v//vn", "v/vt/vn"; 1-based, negatives allowed */
    long v = 0, vt = 0, vn = 0;
    char* endp;
    if (!tok || !*tok) return -1;
    v = strtol(tok, &endp, 10);
    if (endp == tok) return -1;
    if (*endp == '/') {
        endp++;
        if (*endp == '/') {
            endp++;
            vn = strtol(endp, NULL, 10);
        } else {
            vt = strtol(endp, &endp, 10);
            if (*endp == '/') {
                endp++;
                vn = strtol(endp, NULL, 10);
            }
        }
    }
    *out_v  = v  > 0 ? v  - 1 : nv  + (int)v;
    *out_vt = vt > 0 ? (int)vt - 1 : nvt + (int)vt;
    *out_vn = vn > 0 ? (int)vn - 1 : nvn + (int)vn;
    return 0;
}

int gk_mesh_load_obj(gk_mesh* m, const char* asset_path) {
    size_t size = 0;
    unsigned char* raw;
    char* text;
    char* line;
    gk__f3arr vp = {0}, vt = {0}, vn = {0};
    gk_mesh_vertex* verts = NULL;
    gk_u16* idx = NULL;
    int nverts = 0, nidx = 0, cap_verts = 0;
    int rc = -1;

    raw = gk_file_read_all(gk_asset_path(asset_path), &size);
    if (!raw) { GK_ERROR("obj not found: %s\n", asset_path); return -1; }
    text = (char*)raw;

    verts = (gk_mesh_vertex*)malloc(sizeof(gk_mesh_vertex) * 1024);
    if (!verts) { free(raw); return -1; }
    cap_verts = 1024;
    idx = (gk_u16*)malloc(sizeof(gk_u16) * (size_t)cap_verts);   /* grows with verts */

    line = text;
    while (line && *line) {
        char* eol = strchr(line, '\n');
        if (eol) *eol = 0;
        {
            char* p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r') p++;
            if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) {
                float x, y, z;
                if (sscanf(p + 2, "%f %f %f", &x, &y, &z) == 3)
                    gk__f3_push(&vp, x, y, z);
            } else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t')) {
                float u, v;
                if (sscanf(p + 3, "%f %f", &u, &v) == 2)
                    gk__f3_push(&vt, u, v, 0.0f);
            } else if (p[0] == 'v' && p[1] == 'n' && (p[2] == ' ' || p[2] == '\t')) {
                float x, y, z;
                if (sscanf(p + 3, "%f %f %f", &x, &y, &z) == 3)
                    gk__f3_push(&vn, x, y, z);
            } else if (p[0] == 'f' && (p[1] == ' ' || p[1] == '\t')) {
                int corner_v[32], corner_vt[32], corner_vn[32];
                int nc = 0, t;
                char* tok = strtok(p + 2, " \t\r");
                while (tok && nc < 32) {
                    if (gk__corner(tok, vp.n, vt.n, vn.n,
                                   &corner_v[nc], &corner_vt[nc], &corner_vn[nc]) == 0)
                        nc++;
                    tok = strtok(NULL, " \t\r");
                }
                for (t = 1; t + 1 < nc; t++) {
                    int tri[3] = { 0, t, t + 1 };
                    int k;
                    gk_vec3 a, b, c3, fn;
                    if (nverts + 3 > cap_verts) {
                        int ncap = cap_verts * 2;
                        gk_mesh_vertex* nv2 = (gk_mesh_vertex*)realloc(verts, sizeof(gk_mesh_vertex) * (size_t)ncap);
                        gk_u16* ni2;
                        if (!nv2) goto done;
                        verts = nv2;
                        cap_verts = ncap;
                        ni2 = (gk_u16*)realloc(idx, sizeof(gk_u16) * (size_t)cap_verts);
                        if (!ni2) goto done;
                        idx = ni2;
                    }
                    /* flat face normal (cross of first tri) */
                    a = gk_v3(vp.data[corner_v[0]].x, vp.data[corner_v[0]].y, vp.data[corner_v[0]].z);
                    b = gk_v3(vp.data[corner_v[tri[1]]].x, vp.data[corner_v[tri[1]]].y, vp.data[corner_v[tri[1]]].z);
                    c3 = gk_v3(vp.data[corner_v[tri[2]]].x, vp.data[corner_v[tri[2]]].y, vp.data[corner_v[tri[2]]].z);
                    fn = gk_vec3_norm(gk_vec3_cross(gk_vec3_sub(b, a), gk_vec3_sub(c3, a)));
                    for (k = 0; k < 3; k++) {
                        int cv = corner_v[tri[k]];
                        int cu = corner_vt[tri[k]];
                        int cn = corner_vn[tri[k]];
                        gk_mesh_vertex* out = &verts[nverts];
                        out->pos[0] = vp.data[cv].x;
                        out->pos[1] = vp.data[cv].y;
                        out->pos[2] = vp.data[cv].z;
                        if (cn >= 0 && cn < vn.n) {
                            out->nrm[0] = vn.data[cn].x;
                            out->nrm[1] = vn.data[cn].y;
                            out->nrm[2] = vn.data[cn].z;
                        } else {
                            out->nrm[0] = fn.x;
                            out->nrm[1] = fn.y;
                            out->nrm[2] = fn.z;
                        }
                        if (cu >= 0 && cu < vt.n) {
                            out->uv[0] = vt.data[cu].x;
                            out->uv[1] = vt.data[cu].y;
                        } else {
                            out->uv[0] = 0.0f;
                            out->uv[1] = 0.0f;
                        }
                        idx[nverts] = (gk_u16)nverts;
                        nverts++;
                        nidx++;
                    }
                }
            }
        }
        line = eol ? eol + 1 : NULL;
    }

    if (nverts > 0 && nidx > 0) {
        GK_LOG("obj %s: %d verts, %d tris\n", asset_path, nverts, nidx / 3);
        rc = gk_mesh_create(m, verts, nverts, idx, nidx);
    } else {
        GK_ERROR("obj %s: no faces found\n", asset_path);
    }
done:
    free(idx);
    free(verts);
    free(vp.data);
    free(vt.data);
    free(vn.data);
    free(raw);
    return rc;
}
