/* =========================================================================
 * gk_mesh.h — static vertex/index buffers + primitives + OBJ loader
 *
 * Vertex layout (32 bytes, interleaved):
 *     float pos[3]; float nrm[3]; float uv[2];
 *
 * gk_mesh_draw binds the attributes "a_pos", "a_nrm", "a_uv" if the given
 * program declares them. The caller supplies uniforms (u_vp, u_model, ...).
 * ========================================================================= */
#ifndef GK_MESH_H
#define GK_MESH_H

#include "gk.h"
#include <GLES2/gl2.h>
#include "gk_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gk_mesh_vertex {
    float pos[3];
    float nrm[3];
    float uv[2];
} gk_mesh_vertex;

typedef struct gk_mesh {
    GLuint vbo;
    GLuint ibo;
    int    index_count;
} gk_mesh;

/* Both arrays are copied into GPU buffers. Returns 0 on success. */
int  gk_mesh_create(gk_mesh* m, const gk_mesh_vertex* verts, int nv,
                    const gk_u16* indices, int ni);
void gk_mesh_destroy(gk_mesh* m);

/* Binds the mesh's buffers and the shader's a_pos/a_nrm/a_uv attributes,
   then glDrawElements(GL_TRIANGLES). Does NOT change the program. */
void gk_mesh_draw(const gk_mesh* m, const gk_shader* sh);

/* --- primitives (all centered on the origin) ---
 * cube     : edge length `size`, uv tiled `uv_tile` times per face
 * plane    : on the XZ plane, +Y normal, centered, w x d units
 * sphere   : UV sphere, v = 0 at the top pole (matches y-down uv rule)
 */
int gk_mesh_cube(gk_mesh* m, float size, float uv_tile);
int gk_mesh_plane(gk_mesh* m, float w, float d,
                  int seg_w, int seg_d, float u_tile, float v_tile);
int gk_mesh_sphere(gk_mesh* m, float radius, int stacks, int slices);

/* Minimal Wavefront OBJ loader: v / vt / vn / f lines, triangulated.
   Missing normals get flat per-face normals, missing uvs get 0,0.
   Max 21845 source triangles (65535/3 exploded vertices). Returns 0 on success. */
int gk_mesh_load_obj(gk_mesh* m, const char* asset_path);

#ifdef __cplusplus
}
#endif

#endif /* GK_MESH_H */
