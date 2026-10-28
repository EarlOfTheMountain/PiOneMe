/* =========================================================================
 * gk_window.h — fullscreen EGL + Dispmanx display (no Xorg)
 *
 * Creates an opaque GLES2 surface layered above the console via the
 * Broadcom Dispmanx compositor. On destroy the console layer is restored.
 * ========================================================================= */
#ifndef GK_WINDOW_H
#define GK_WINDOW_H

#include "gk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gk_window gk_window;

/* width/height of 0,0 => render at the native display resolution.
   A non-zero request renders at that resolution and Dispmanx upscales
   the element to the full screen (fast low-res mode).
   bcm_host_init() is called here. Returns 0 on success. */
int  gk_window_create(gk_window** out, int width, int height);

void gk_window_swap(gk_window* win);        /* eglSwapBuffers, vsync'd */
void gk_window_make_current(gk_window* win);

int   gk_window_width(const gk_window* win);         /* render resolution  */
int   gk_window_height(const gk_window* win);
int   gk_window_screen_width(const gk_window* win);  /* physical display   */
int   gk_window_screen_height(const gk_window* win);
float gk_window_aspect(const gk_window* win);

void gk_window_destroy(gk_window* win);

#ifdef __cplusplus
}
#endif

#endif /* GK_WINDOW_H */
