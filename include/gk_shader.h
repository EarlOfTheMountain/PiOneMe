/* =========================================================================
 * gk_shader.h — GLSL program loading with uniform/attribute caching
 *
 * gk_shader_load("shaders/lit") reads:
 *     assets/shaders/lit.vert
 *     assets/shaders/lit.frag
 * and links them. Uniform and attribute locations are cached by name.
 * ========================================================================= */
#ifndef GK_SHADER_H
#define GK_SHADER_H

#include "gk.h"
#include <GLES2/gl2.h>
#include "gk_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gk_shader gk_shader;

/* Compile + link from source strings. Returns NULL and logs on failure. */
gk_shader* gk_shader_create(const char* vs_src, const char* fs_src);

/* Load "<path>.vert" + "<path>.frag" from the asset root.
   Returns NULL (and logs) when either file is missing or fails to compile. */
gk_shader* gk_shader_load(const char* path);

void   gk_shader_destroy(gk_shader* sh);
GLuint gk_shader_program(const gk_shader* sh);
void   gk_shader_use(const gk_shader* sh);

/* Location lookups (cached). -1 if not present in the program. */
GLint gk_shader_uniform(const gk_shader* sh, const char* name);
GLint gk_shader_attrib (const gk_shader* sh, const char* name);

/* Convenience setters (also make the program current). */
void gk_shader_set_mat4 (gk_shader* sh, const char* name, const gk_mat4* m);
void gk_shader_set_vec2 (gk_shader* sh, const char* name, float x, float y);
void gk_shader_set_vec3 (gk_shader* sh, const char* name, float x, float y, float z);
void gk_shader_set_vec4 (gk_shader* sh, const char* name, float x, float y, float z, float w);
void gk_shader_set_float(gk_shader* sh, const char* name, float v);
void gk_shader_set_int  (gk_shader* sh, const char* name, int v);

#ifdef __cplusplus
}
#endif

#endif /* GK_SHADER_H */
