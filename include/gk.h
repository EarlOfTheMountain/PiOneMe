/* =========================================================================
 * gk.h — GameKit common definitions
 *
 * A minimal GLES2 game framework for the Raspberry Pi (BCM2835 / VideoCore
 * IV, Dispmanx, no Xorg). Targets Raspbian Buster with the legacy vendor
 * libraries in /opt/vc.
 *
 *   #include "gk.h"
 *
 * ========================================================================= */
#ifndef GK_H
#define GK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- version ---------------------------------------------------------- */
#define GK_VERSION_MAJOR 0
#define GK_VERSION_MINOR 1
#define GK_VERSION_STRING "gamekit 0.1 (GLES2 / Dispmanx)"

/* ---- basic types ------------------------------------------------------ */
typedef float  gk_f32;
typedef double gk_f64;
typedef int8_t   gk_i8;
typedef int16_t  gk_i16;
typedef int32_t  gk_i32;
typedef int64_t  gk_i64;
typedef uint8_t  gk_u8;
typedef uint16_t gk_u16;
typedef uint32_t gk_u32;
typedef uint64_t gk_u64;

/* ---- logging / diagnostics -------------------------------------------- */
#define GK_LOG(...)   do { fprintf(stdout, "[gk] " __VA_ARGS__);  fflush(stdout); } while (0)
#define GK_WARN(...)  do { fprintf(stderr, "[gk] warn: " __VA_ARGS__);  fflush(stderr); } while (0)
#define GK_ERROR(...) do { fprintf(stderr, "[gk] error: " __VA_ARGS__); fflush(stderr); } while (0)

#define GK_ASSERT(c) \
    do { if (!(c)) { GK_ERROR("assert failed: %s (%s:%d)\n", #c, __FILE__, __LINE__); abort(); } } while (0)

#ifdef GK_DEBUG
void gk__gl_check(const char* file, int line);
#define GK_CHECK_GL() gk__gl_check(__FILE__, __LINE__)
#else
#define GK_CHECK_GL() do {} while (0)
#endif

const char* gk_gl_error_string(int err);

/* ---- scalar helpers ---------------------------------------------------- */
static inline float gk_minf(float a, float b) { return a < b ? a : b; }
static inline float gk_maxf(float a, float b) { return a > b ? a : b; }
static inline float gk_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float gk_lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float gk_degf(float rad) { return rad * 57.2957795131f; }
static inline float gk_radf(float deg) { return deg * 0.0174532925199f; }

/* frame-rate independent smoothing towards a target */
static inline float gk_smooth(float cur, float target, float rate, float dt) {
    return cur + (target - cur) * (1.0f - expf(-rate * dt));
}

static inline float gk_rand01(void) { return (float)rand() / (float)(RAND_MAX + 1u); }
static inline float gk_rand_range(float lo, float hi) { return lo + (hi - lo) * gk_rand01(); }
static inline int   gk_rand_int(int lo, int hi) { return lo + (int)(gk_rand01() * (float)(hi - lo + 1)); }

/* ---- small file helpers ------------------------------------------------ */
/* Reads a whole file into a malloc'd buffer with one extra NUL byte.
   Returns NULL and sets *out_size = 0 on failure. Free with free(). */
unsigned char* gk_file_read_all(const char* path, size_t* out_size);
bool gk_file_exists(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* GK_H */
