/* gk_shader.c — GLSL program compilation + uniform/attribute caches */
#include "gk.h"
#include "gk_shader.h"
#include "gk_assets.h"

#include <stdio.h>

#define GK_MAX_UNIFORMS 32
#define GK_MAX_ATTRIBS  12

typedef struct {
    char  name[32];
    GLint loc;
} gk__loc;

struct gk_shader {
    GLuint   prog;
    gk__loc  uni[GK_MAX_UNIFORMS];
    int      n_uni;
    gk__loc  attr[GK_MAX_ATTRIBS];
    int      n_attr;
};

static GLuint gk__compile(GLenum type, const char* src, const char* label) {
    GLuint sh = glCreateShader(type);
    GLint len = (GLint)strlen(src);
    GLint ok = GL_FALSE;
    char log[1024];

    glShaderSource(sh, 1, &src, &len);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLsizei n = 0;
        glGetShaderInfoLog(sh, sizeof log - 1, &n, log);
        log[n] = 0;
        GK_ERROR("shader compile failed (%s):\n%s\n", label, log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

gk_shader* gk_shader_create(const char* vs_src, const char* fs_src) {
    gk_shader* sh;
    GLuint vs, fs, prog;
    GLint ok = GL_FALSE;
    char log[1024];
    GLsizei n = 0;

    if (!vs_src || !fs_src) return NULL;

    vs = gk__compile(GL_VERTEX_SHADER, vs_src, "vertex");
    fs = gk__compile(GL_FRAGMENT_SHADER, fs_src, "fragment");
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return NULL;
    }

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(prog, sizeof log - 1, &n, log);
        log[n] = 0;
        GK_ERROR("program link failed:\n%s\n", log);
        glDeleteProgram(prog);
        return NULL;
    }

    sh = (gk_shader*)calloc(1, sizeof(gk_shader));
    if (!sh) { glDeleteProgram(prog); return NULL; }
    sh->prog = prog;
    return sh;
}

gk_shader* gk_shader_load(const char* path) {
    char tmp[512];
    unsigned char* vs_data;
    unsigned char* fs_data;
    gk_shader* sh;

    if (!path) return NULL;
    snprintf(tmp, sizeof tmp, "%.500s.vert", path);
    vs_data = gk_file_read_all(gk_asset_path(tmp), NULL);
    snprintf(tmp, sizeof tmp, "%.500s.frag", path);
    fs_data = gk_file_read_all(gk_asset_path(tmp), NULL);

    if (!vs_data || !fs_data) {
        GK_WARN("shader files missing for \"%s\"%s%s\n", path,
                vs_data ? "" : " (.vert)", fs_data ? "" : " (.frag)");
        free(vs_data);
        free(fs_data);
        return NULL;
    }
    sh = gk_shader_create((const char*)vs_data, (const char*)fs_data);
    free(vs_data);
    free(fs_data);
    return sh;
}

void gk_shader_destroy(gk_shader* sh) {
    if (!sh) return;
    glDeleteProgram(sh->prog);
    free(sh);
}

GLuint gk_shader_program(const gk_shader* sh) { return sh ? sh->prog : 0; }

void gk_shader_use(const gk_shader* sh) {
    glUseProgram(sh ? sh->prog : 0);
}

static GLint gk__uniform_lookup(const gk_shader* sh, const char* name) {
    /* the location cache is mutable state; cast away const deliberately */
    gk_shader* m = (gk_shader*)sh;
    int i;
    for (i = 0; i < m->n_uni; i++)
        if (strcmp(m->uni[i].name, name) == 0)
            return m->uni[i].loc;
    if (m->n_uni < GK_MAX_UNIFORMS) {
        GLint loc = glGetUniformLocation(m->prog, name);
        snprintf(m->uni[m->n_uni].name, sizeof m->uni[0].name, "%.31s", name);
        m->uni[m->n_uni].loc = loc;
        m->n_uni++;
        return loc;
    }
    return glGetUniformLocation(m->prog, name);
}

static GLint gk__attrib_lookup(const gk_shader* sh, const char* name) {
    gk_shader* m = (gk_shader*)sh;
    int i;
    for (i = 0; i < m->n_attr; i++)
        if (strcmp(m->attr[i].name, name) == 0)
            return m->attr[i].loc;
    if (m->n_attr < GK_MAX_ATTRIBS) {
        GLint loc = glGetAttribLocation(m->prog, name);
        snprintf(m->attr[m->n_attr].name, sizeof m->attr[0].name, "%.31s", name);
        m->attr[m->n_attr].loc = loc;
        m->n_attr++;
        return loc;
    }
    return glGetAttribLocation(m->prog, name);
}

GLint gk_shader_uniform(const gk_shader* sh, const char* name) {
    return sh ? gk__uniform_lookup(sh, name) : -1;
}

GLint gk_shader_attrib(const gk_shader* sh, const char* name) {
    return sh ? gk__attrib_lookup(sh, name) : -1;
}

void gk_shader_set_mat4(gk_shader* sh, const char* name, const gk_mat4* m) {
    GLint loc = gk__uniform_lookup(sh, name);
    if (loc < 0 || !m) return;
    glUseProgram(sh->prog);
    glUniformMatrix4fv(loc, 1, GL_FALSE, m->m);
}

void gk_shader_set_vec2(gk_shader* sh, const char* name, float x, float y) {
    GLint loc = gk__uniform_lookup(sh, name);
    if (loc < 0) return;
    glUseProgram(sh->prog);
    glUniform2f(loc, x, y);
}

void gk_shader_set_vec3(gk_shader* sh, const char* name, float x, float y, float z) {
    GLint loc = gk__uniform_lookup(sh, name);
    if (loc < 0) return;
    glUseProgram(sh->prog);
    glUniform3f(loc, x, y, z);
}

void gk_shader_set_vec4(gk_shader* sh, const char* name, float x, float y, float z, float w) {
    GLint loc = gk__uniform_lookup(sh, name);
    if (loc < 0) return;
    glUseProgram(sh->prog);
    glUniform4f(loc, x, y, z, w);
}

void gk_shader_set_float(gk_shader* sh, const char* name, float v) {
    GLint loc = gk__uniform_lookup(sh, name);
    if (loc < 0) return;
    glUseProgram(sh->prog);
    glUniform1f(loc, v);
}

void gk_shader_set_int(gk_shader* sh, const char* name, int v) {
    GLint loc = gk__uniform_lookup(sh, name);
    if (loc < 0) return;
    glUseProgram(sh->prog);
    glUniform1i(loc, v);
}
