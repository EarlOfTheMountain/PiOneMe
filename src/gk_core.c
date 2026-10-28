/* gk_core.c — small shared utilities: file IO, GL error names */
#include "gk.h"

#include <GLES2/gl2.h>
#include <sys/stat.h>

unsigned char* gk_file_read_all(const char* path, size_t* out_size) {
    FILE* f;
    long sz;
    unsigned char* buf;

    if (out_size) *out_size = 0;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    buf = (unsigned char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[sz] = 0;
    if (out_size) *out_size = (size_t)sz;
    return buf;
}

bool gk_file_exists(const char* path) {
    struct stat st;
    if (!path) return false;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

const char* gk_gl_error_string(int err) {
    switch (err) {
    case GL_NO_ERROR:                      return "GL_NO_ERROR";
    case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
    default:                               return "unknown GL error";
    }
}

#ifdef GK_DEBUG
#include <GLES2/gl2ext.h>
void gk__gl_check(const char* file, int line) {
    GLenum err = glGetError();
    while (err != GL_NO_ERROR) {
        GK_ERROR("GL error %s at %s:%d\n", gk_gl_error_string((int)err), file, line);
        err = glGetError();
    }
}
#endif
