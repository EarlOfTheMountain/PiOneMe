/* =========================================================================
 * gk_window.c — EGL + Dispmanx fullscreen display for the legacy
 * Raspberry Pi VideoCore stack (/opt/vc). Mirrors the approach of the
 * /opt/vc/src/hello_pi/hello_triangle sample.
 * ========================================================================= */
#include "gk.h"
#include "gk_window.h"

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

struct gk_window {
    EGLDisplay dpy;
    EGLSurface surf;
    EGLContext ctx;
    EGLConfig  cfg;

    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;

    uint32_t screen_w, screen_h;   /* physical display size  */
    int w, h;                      /* render resolution      */
};

int gk_window_create(gk_window** out, int width, int height) {
    gk_window* win;
    static const EGLint cfg_attr[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    static const EGLint ctx_attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLint num_cfg = 0;
    VC_RECT_T dst_rect, src_rect;
    DISPMANX_UPDATE_HANDLE_T update;
    EGL_DISPMANX_WINDOW_T native_window;

    *out = NULL;
    win = (gk_window*)calloc(1, sizeof(gk_window));
    if (!win) { GK_ERROR("out of memory\n"); return -1; }

    bcm_host_init();

    if (graphics_get_display_size(0, &win->screen_w, &win->screen_h) < 0) {
        GK_ERROR("graphics_get_display_size failed\n");
        free(win);
        return -1;
    }
    win->w = width  > 0 ? width  : (int)win->screen_w;
    win->h = height > 0 ? height : (int)win->screen_h;

    win->dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (win->dpy == EGL_NO_DISPLAY) {
        GK_ERROR("eglGetDisplay failed\n");
        free(win);
        return -1;
    }
    if (!eglInitialize(win->dpy, NULL, NULL)) {
        GK_ERROR("eglInitialize failed\n");
        free(win);
        return -1;
    }
    if (!eglChooseConfig(win->dpy, cfg_attr, &win->cfg, 1, &num_cfg) || num_cfg < 1) {
        GK_ERROR("eglChooseConfig: no suitable config\n");
        eglTerminate(win->dpy);
        free(win);
        return -1;
    }
    win->ctx = eglCreateContext(win->dpy, win->cfg, EGL_NO_CONTEXT, ctx_attr);
    if (win->ctx == EGL_NO_CONTEXT) {
        GK_ERROR("eglCreateContext failed\n");
        eglTerminate(win->dpy);
        free(win);
        return -1;
    }

    /* Dispmanx element: source rect = render resolution, destination =
       full screen. Dispmanx scales for us, so a small render resolution
       (e.g. 640x360) still covers the whole display. */
    dst_rect.x = 0; dst_rect.y = 0;
    dst_rect.width  = (int32_t)win->screen_w;
    dst_rect.height = (int32_t)win->screen_h;
    src_rect.x = 0; src_rect.y = 0;
    src_rect.width  = win->w;
    src_rect.height = win->h;

    win->dispman_display = vc_dispmanx_display_open(0 /* LCD/HDMI */);
    update = vc_dispmanx_update_start(0);
    win->dispman_element = vc_dispmanx_element_add(
        update, win->dispman_display,
        0 /* layer, above the console */,
        &dst_rect, 0 /* source resource handle */,
        &src_rect,
        DISPMANX_PROTECTION_NONE,
        0 /* VC_DISPMANX_ALPHA_T* */,
        0 /* clamp */,
        (DISPMANX_TRANSFORM_T)0);
    vc_dispmanx_update_submit_sync(update);

    if (win->dispman_element == DISPMANX_NO_HANDLE) {
        GK_ERROR("vc_dispmanx_element_add failed (need video group or root?)\n");
        vc_dispmanx_display_close(win->dispman_display);
        eglDestroyContext(win->dpy, win->ctx);
        eglTerminate(win->dpy);
        free(win);
        return -1;
    }

    native_window.element = win->dispman_element;
    native_window.width   = win->w;
    native_window.height  = win->h;

    win->surf = eglCreateWindowSurface(win->dpy, win->cfg,
                                       (EGLNativeWindowType)&native_window, NULL);
    if (win->surf == EGL_NO_SURFACE) {
        GK_ERROR("eglCreateWindowSurface failed\n");
        goto fail;
    }
    if (!eglMakeCurrent(win->dpy, win->surf, win->surf, win->ctx)) {
        GK_ERROR("eglMakeCurrent failed\n");
        goto fail;
    }
    eglSwapInterval(win->dpy, 1);   /* vsync */

    glViewport(0, 0, win->w, win->h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    GK_LOG("display %ux%u, rendering at %dx%d (EGL %s / GLES2)\n",
           win->screen_w, win->screen_h, win->w, win->h,
           eglQueryString(win->dpy, EGL_VENDOR) ? eglQueryString(win->dpy, EGL_VENDOR) : "?");

    *out = win;
    return 0;

fail:
    update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(update, win->dispman_element);
    vc_dispmanx_update_submit_sync(update);
    vc_dispmanx_display_close(win->dispman_display);
    eglDestroyContext(win->dpy, win->ctx);
    eglTerminate(win->dpy);
    free(win);
    return -1;
}

void gk_window_swap(gk_window* win) {
    if (win) eglSwapBuffers(win->dpy, win->surf);
}

void gk_window_make_current(gk_window* win) {
    if (win) eglMakeCurrent(win->dpy, win->surf, win->surf, win->ctx);
}

int gk_window_width(const gk_window* win)  { return win ? win->w : 0; }
int gk_window_height(const gk_window* win) { return win ? win->h : 0; }
int gk_window_screen_width(const gk_window* win)  { return win ? (int)win->screen_w : 0; }
int gk_window_screen_height(const gk_window* win) { return win ? (int)win->screen_h : 0; }

float gk_window_aspect(const gk_window* win) {
    if (!win || win->h == 0) return 1.0f;
    return (float)win->w / (float)win->h;
}

void gk_window_destroy(gk_window* win) {
    DISPMANX_UPDATE_HANDLE_T update;
    if (!win) return;
    eglMakeCurrent(win->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(win->dpy, win->surf);
    eglDestroyContext(win->dpy, win->ctx);
    eglTerminate(win->dpy);

    update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(update, win->dispman_element);
    vc_dispmanx_update_submit_sync(update);
    vc_dispmanx_display_close(win->dispman_display);

    bcm_host_deinit();
    free(win);
}
