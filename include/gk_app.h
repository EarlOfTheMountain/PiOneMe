/* =========================================================================
 * gk_app.h — application lifecycle and main loop
 *
 *     bool init(game_t* g);          called once, GL context is current
 *     void update(game_t* g, float dt);
 *     void render(game_t* g);
 *     void shutdown(game_t* g);
 *
 * gk_app_run() wires up: asset root, EGL/Dispmanx window, evdev input,
 * ALSA audio, signal handling (SIGINT/SIGTERM quit cleanly), delta time
 * and the frame loop. Returns 0 on a clean exit.
 * ========================================================================= */
#ifndef GK_APP_H
#define GK_APP_H

#include "gk.h"
#include "gk_math.h"
#include "gk_window.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gk_app gk_app;

struct gk_app {
    const char* title;          /* unused by Dispmanx, kept for logging */
    int   width, height;        /* 0,0 => native display resolution     */
    gk_color clear;             /* used as the default clear color      */
    bool  quit_on_esc;          /* ESC exits the app                    */
    bool  use_audio;            /* initialize the ALSA mixer            */

    void* user;                 /* your state, passed to callbacks      */
    bool  (*init)(gk_app* app);
    void  (*update)(gk_app* app, float dt);
    void  (*render)(gk_app* app);
    void  (*shutdown)(gk_app* app);
};

int   gk_app_run(gk_app* app);
void  gk_app_quit(void);          /* request exit from anywhere */
float gk_app_dt(void);            /* seconds, clamped to 0.1    */
float gk_app_fps(void);           /* smoothed                   */
float gk_app_time(void);          /* seconds since gk_app_run   */
gk_window* gk_app_window(void);

#ifdef __cplusplus
}
#endif

#endif /* GK_APP_H */
