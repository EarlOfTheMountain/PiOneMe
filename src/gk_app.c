/* gk_app.c — application lifecycle, main loop, signals, timing */
#include "gk.h"
#include "gk_app.h"
#include "gk_assets.h"
#include "gk_input.h"
#include "gk_audio.h"

#include <signal.h>
#include <time.h>
#include <GLES2/gl2.h>

static volatile sig_atomic_t gk__quit = 0;
static gk_window* gk__win = NULL;
static float gk__dt = 0.0f;
static float gk__fps = 0.0f;
static float gk__time = 0.0f;
static struct timespec gk__t0;

static void gk__on_signal(int sig) {
    (void)sig;
    gk__quit = 1;
}

static float gk__now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (float)ts.tv_sec + (float)ts.tv_nsec * 1e-9f;
}

void gk_app_quit(void) { gk__quit = 1; }
float gk_app_dt(void)   { return gk__dt; }
float gk_app_fps(void)  { return gk__fps; }
float gk_app_time(void) { return gk__time; }
gk_window* gk_app_window(void) { return gk__win; }

int gk_app_run(gk_app* app) {
    float prev;
    int rc = 0;
    struct sigaction sa;

    if (!app) return -1;
    gk__quit = 0;
    gk__dt = 0.0f;
    gk__fps = 0.0f;
    gk__time = 0.0f;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = gk__on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    GK_LOG("%s\n", GK_VERSION_STRING);
    gk_assets_init();

    if (gk_window_create(&gk__win, app->width, app->height) != 0) {
        return 1;
    }
    gk_input_init();
    gk_input_set_screen_size(gk_window_width(gk__win), gk_window_height(gk__win));

    if (app->use_audio)
        gk_audio_init();

    glClearColor(app->clear.r, app->clear.g, app->clear.b, app->clear.a);

    clock_gettime(CLOCK_MONOTONIC, &gk__t0);
    prev = gk__now_seconds();

    if (app->init && !app->init(app)) {
        GK_ERROR("app init failed\n");
        rc = 1;
        goto cleanup;
    }

    while (!gk__quit) {
        float now = gk__now_seconds();
        gk__dt = now - prev;
        if (gk__dt < 0.0f) gk__dt = 0.0f;
        if (gk__dt > 0.1f) gk__dt = 0.1f;
        prev = now;
        gk__time = now - ((float)gk__t0.tv_sec + (float)gk__t0.tv_nsec * 1e-9f);
        gk__fps = gk__fps > 0.0f
                ? gk_lerpf(gk__fps, 1.0f / (gk__dt > 1e-5f ? gk__dt : 1e-5f), 0.05f)
                : 1.0f / (gk__dt > 1e-5f ? gk__dt : 1e-5f);

        gk_input_poll();

        if (app->quit_on_esc && gk_key_pressed(GK_KEY_ESCAPE))
            gk__quit = 1;

        if (app->update) app->update(app, gk__dt);
        if (app->render) app->render(app);

        gk_window_swap(gk__win);
    }

cleanup:
    if (app->shutdown) app->shutdown(app);
    gk_audio_shutdown();
    gk_input_shutdown();
    gk_window_destroy(gk__win);
    gk__win = NULL;
    return rc;
}
