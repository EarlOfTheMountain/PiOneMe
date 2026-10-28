/* =========================================================================
 * GK Runner — GameKit demo game
 *
 * A 3-lane endless dodger that exercises the whole framework:
 *   - lit textured meshes with depth fog (lit.vert/.frag)
 *   - unlit meshes for debris particles
 *   - an OBJ-loaded model (pylons)
 *   - scrolling UVs on the road (shader uniform)
 *   - evdev input: keyboard + gamepad
 *   - ALSA mixer: sfx + looping music
 *   - sprite batch + 8x8 font HUD
 *   - shader hot reload with R
 *
 * Controls: LEFT/RIGHT or A/D (or pad dpad / left stick) to switch lanes,
 * ENTER (or pad A/START) to start/restart, R reloads shaders, ESC quits.
 * ========================================================================= */
#include "gk.h"
#include "gk_app.h"
#include "gk_window.h"
#include "gk_math.h"
#include "gk_shader.h"
#include "gk_texture.h"
#include "gk_mesh.h"
#include "gk_sprite.h"
#include "gk_font.h"
#include "gk_input.h"
#include "gk_audio.h"

#include <stdio.h>

#define LANES 3
static const float LANE_X[LANES] = { -2.2f, 0.0f, 2.2f };

#define MAX_OBST    24
#define MAX_DEBRIS  40
#define PYLON_ROWS  12
#define PYLON_GAP   16.0f
#define ROAD_LEN    132.0f
#define ROAD_ZOFF   (-54.0f)     /* road spans z: -120 .. +12 */
#define ROAD_UV_TILES 33.0f      /* one texture tile = 4 m    */
#define MAX_SPEED   46.0f

typedef enum { ST_TITLE, ST_RUN, ST_OVER } state_t;

typedef struct {
    float x, z;
    bool  alive;
    gk_color tint;
    float spin;
} obst_t;

typedef struct {
    bool    active;
    gk_vec3 pos, vel;
    float   spin, scale, life;
} debris_t;

typedef struct {
    state_t state;
    float t, speed, distance, hi;
    int   lane;
    float lane_x;
    int   lives;
    float invuln;
    bool  flash;
    float shake;
    float spawn_gap, spawn_acc;
    float uv_scroll;
    float ball_rot;
    float prev_ax;

    obst_t   obst[MAX_OBST];
    debris_t debris[MAX_DEBRIS];
    float    pylon_z[PYLON_ROWS];

    gk_shader* lit;
    gk_shader* unlit;
    gk_texture road_tex, crate_tex, sphere_tex, pylon_tex;
    gk_mesh    road, crate, ball, pylon, debris_mesh;
    gk_sprite* hud;
    gk_sound *s_switch, *s_crash, *s_start, *s_over, *music;
    int music_ch;

    gk_vec3 light_dir, light_col;
    gk_color fog_col;
    float   ambient;
    int   sw, sh;
    float hud_scale;
} game_t;

/* ---- high score (kept in $HOME) ------------------------------------------ */
static void gk__load_hi(game_t* g) {
    const char* home = getenv("HOME");
    g->hi = 0.0f;
    if (home) {
        char path[512];
        FILE* f;
        snprintf(path, sizeof path, "%.400s/.gkrunner_hi", home);
        f = fopen(path, "r");
        if (f) {
            float v = 0.0f;
            if (fscanf(f, "%f", &v) == 1) g->hi = v;
            fclose(f);
        }
    }
}

static void gk__save_hi(game_t* g) {
    const char* home = getenv("HOME");
    if (home) {
        char path[512];
        FILE* f;
        snprintf(path, sizeof path, "%.400s/.gkrunner_hi", home);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "%.0f\n", g->hi);
            fclose(f);
        }
    }
}

/* ---- game state ------------------------------------------------------------ */

static void reset_run(game_t* g) {
    int i;
    g->state = ST_TITLE;
    g->t = 0.0f;
    g->speed = 14.0f;
    g->distance = 0.0f;
    g->lane = 1;
    g->lane_x = LANE_X[1];
    g->lives = 3;
    g->invuln = 1.2f;
    g->flash = false;
    g->shake = 0.0f;
    g->spawn_gap = 26.0f;
    g->spawn_acc = 14.0f;
    g->uv_scroll = 0.0f;
    g->ball_rot = 0.0f;
    g->prev_ax = 0.0f;
    g->music_ch = -1;
    for (i = 0; i < MAX_OBST; i++)   g->obst[i].alive = false;
    for (i = 0; i < MAX_DEBRIS; i++) g->debris[i].active = false;
    for (i = 0; i < PYLON_ROWS; i++) g->pylon_z[i] = -20.0f - (float)i * PYLON_GAP;
}

static void spawn_obstacle(game_t* g, int lane, float z) {
    int i;
    (void)lane;   /* position comes from LANE_X below; kept for future patterns */
    for (i = 0; i < MAX_OBST; i++) {
        if (!g->obst[i].alive) {
            g->obst[i].alive = true;
            g->obst[i].x = LANE_X[lane];
            g->obst[i].z = z;
            g->obst[i].spin = gk_rand_range(0.0f, 6.28f);
            switch (gk_rand_int(0, 2)) {
            case 0:  g->obst[i].tint = gk_col(1.0f, 0.42f, 0.36f, 1.0f); break;
            case 1:  g->obst[i].tint = gk_col(0.95f, 0.72f, 0.30f, 1.0f); break;
            default: g->obst[i].tint = gk_col(0.45f, 0.72f, 1.0f, 1.0f); break;
            }
            return;
        }
    }
}

static void spawn_debris(game_t* g, float x, float y, float z, int n) {
    int i;
    for (i = 0; i < MAX_DEBRIS && n > 0; i++) {
        debris_t* d = &g->debris[i];
        if (!d->active) {
            d->active = true;
            d->pos = gk_v3(x + gk_rand_range(-0.3f, 0.3f), y, z);
            d->vel = gk_v3(gk_rand_range(-4.0f, 4.0f),
                             gk_rand_range(2.0f, 7.0f),
                             gk_rand_range(-1.0f, 5.0f));
            d->spin = gk_rand_range(-8.0f, 8.0f);
            d->scale = gk_rand_range(0.6f, 1.5f);
            d->life = gk_rand_range(0.9f, 1.4f);
            n--;
        }
    }
}

static void start_run(game_t* g) {
    reset_run(g);
    g->state = ST_RUN;
    gk_sound_play(g->s_start, 0.8f, 0.0f, false);
    g->music_ch = gk_sound_play(g->music, 0.5f, 0.0f, true);
}

static void reload_shaders(game_t* g) {
    gk_shader* nl = gk_shader_load("shaders/lit");
    gk_shader* nu = gk_shader_load("shaders/unlit");
    if (nl && nu) {
        if (g->lit)   gk_shader_destroy(g->lit);
        if (g->unlit) gk_shader_destroy(g->unlit);
        g->lit = nl;
        g->unlit = nu;
        GK_LOG("shaders reloaded\n");
    } else {
        if (nl) gk_shader_destroy(nl);
        if (nu) gk_shader_destroy(nu);
        GK_WARN("shader reload failed - keeping current programs\n");
    }
}

/* ---- lifecycle -------------------------------------------------------------- */

static bool game_init(gk_app* app) {
    game_t* g = (game_t*)calloc(1, sizeof(game_t));
    if (!g) return false;
    app->user = g;

    g->sw = gk_window_width(gk_app_window());
    g->sh = gk_window_height(gk_app_window());
    g->hud_scale = gk_maxf(2.0f, (float)g->sh / 270.0f);

    g->lit = gk_shader_load("shaders/lit");
    g->unlit = gk_shader_load("shaders/unlit");
    if (!g->lit || !g->unlit) {
        GK_ERROR("required shaders missing from assets/shaders\n");
        return false;
    }

    if (gk_texture_load(&g->road_tex, "images/road.png", GK_FILTER_LINEAR, GK_WRAP_REPEAT) != 0)
        return false;
    gk_texture_load(&g->crate_tex,  "images/crate.png",  GK_FILTER_LINEAR, GK_WRAP_CLAMP);
    gk_texture_load(&g->sphere_tex, "images/sphere.png", GK_FILTER_LINEAR, GK_WRAP_REPEAT);
    gk_texture_load(&g->pylon_tex,  "images/pylon.png",  GK_FILTER_LINEAR, GK_WRAP_CLAMP);

    if (gk_mesh_plane(&g->road, 8.0f, ROAD_LEN, 1, 33, 1.0f, ROAD_UV_TILES) != 0)
        return false;
    gk_mesh_cube(&g->crate, 1.6f, 1.0f);
    gk_mesh_sphere(&g->ball, 0.55f, 12, 18);
    gk_mesh_cube(&g->debris_mesh, 0.3f, 0.5f);
    if (gk_mesh_load_obj(&g->pylon, "models/pylon.obj") != 0)
        GK_WARN("pylon.obj failed to load (continuing without it)\n");

    g->hud = gk_sprite_create(1024);
    if (!g->hud) return false;

    g->s_switch = gk_sound_load("sounds/switch.wav");
    g->s_crash  = gk_sound_load("sounds/crash.wav");
    g->s_start  = gk_sound_load("sounds/start.wav");
    g->s_over   = gk_sound_load("sounds/over.wav");
    g->music    = gk_sound_load("sounds/music.wav");

    g->light_dir = gk_vec3_norm(gk_v3(0.4f, -1.0f, -0.3f));
    g->light_col = gk_v3(1.0f, 0.97f, 0.88f);
    g->fog_col   = gk_col(0.55f, 0.72f, 0.92f, 1.0f);
    g->ambient   = 0.34f;

    gk__load_hi(g);
    reset_run(g);
    GK_LOG("GK Runner ready (%dx%d, pads: %d)\n", g->sw, g->sh, gk_pad_count());
    return true;
}

static void game_update(gk_app* app, float dt) {
    game_t* g = (game_t*)app->user;
    int i;

    g->shake = gk_maxf(0.0f, g->shake - dt * 1.5f);

    /* shader hot reload */
    if (gk_key_pressed(GK_KEY_R))
        reload_shaders(g);

    /* steering (keyboard, dpad, left stick with edge detection) */
    {
        bool left = gk_key_pressed(GK_KEY_LEFT) || gk_key_pressed(GK_KEY_A) ||
                    gk_pad_pressed(0, GK_PAD_LEFT);
        bool right = gk_key_pressed(GK_KEY_RIGHT) || gk_key_pressed(GK_KEY_D) ||
                     gk_pad_pressed(0, GK_PAD_RIGHT);
        float ax = gk_pad_axis(0, 0);
        if (ax < -0.5f && g->prev_ax >= -0.5f) left = true;
        if (ax >  0.5f && g->prev_ax <=  0.5f) right = true;
        g->prev_ax = ax;

        if (left && g->lane > 0) {
            g->lane--;
            gk_sound_play(g->s_switch, 0.55f, -0.4f, false);
        }
        if (right && g->lane < LANES - 1) {
            g->lane++;
            gk_sound_play(g->s_switch, 0.55f, 0.4f, false);
        }
    }

    if (g->state == ST_TITLE || g->state == ST_OVER) {
        if (gk_key_pressed(GK_KEY_RETURN) || gk_pad_pressed(0, GK_PAD_A) ||
            gk_pad_pressed(0, GK_PAD_START))
            start_run(g);
        return;
    }

    /* ---- running ---- */
    g->t += dt;
    g->speed = gk_minf(MAX_SPEED, 14.0f + g->t * 1.1f);
    g->distance += g->speed * dt;
    g->lane_x = gk_smooth(g->lane_x, LANE_X[g->lane], 10.0f, dt);
    g->uv_scroll -= g->speed * dt * (ROAD_UV_TILES / ROAD_LEN);
    g->ball_rot -= g->speed * dt / 0.55f;

    /* spawning */
    g->spawn_acc += g->speed * dt;
    if (g->spawn_acc >= g->spawn_gap) {
        g->spawn_acc = 0.0f;
        g->spawn_gap = gk_maxf(13.0f, 26.0f - g->t * 0.12f);
        spawn_obstacle(g, gk_rand_int(0, LANES - 1), -118.0f);
        if (g->speed > 24.0f && gk_rand01() < 0.35f) {
            int l = gk_rand_int(0, LANES - 1);
            spawn_obstacle(g, l, -118.0f - gk_rand_range(2.0f, 8.0f));
        }
    }

    /* obstacles */
    for (i = 0; i < MAX_OBST; i++) {
        obst_t* o = &g->obst[i];
        if (!o->alive) continue;
        o->z += g->speed * dt;
        o->spin += dt * 0.7f;
        if (o->z > 10.0f) { o->alive = false; continue; }
        if (g->invuln <= 0.0f &&
            fabsf(g->lane_x - o->x) < 1.05f && fabsf(0.0f - o->z) < 1.05f) {
            o->alive = false;
            g->lives--;
            g->invuln = 1.6f;
            g->shake = 0.7f;
            gk_sound_play(g->s_crash, 0.9f, o->x * 0.3f, false);
            spawn_debris(g, o->x, 1.0f, o->z, 12);
            if (g->lives <= 0) {
                if (g->distance > g->hi) {
                    g->hi = g->distance;
                    gk__save_hi(g);
                }
                g->state = ST_OVER;
                if (g->music_ch >= 0) gk_channel_stop(g->music_ch);
                gk_sound_play(g->s_over, 0.8f, 0.0f, false);
            }
        }
    }

    /* pylons recycle */
    for (i = 0; i < PYLON_ROWS; i++) {
        g->pylon_z[i] += g->speed * dt;
        if (g->pylon_z[i] > 10.0f)
            g->pylon_z[i] -= (float)PYLON_ROWS * PYLON_GAP;
    }

    /* debris */
    for (i = 0; i < MAX_DEBRIS; i++) {
        debris_t* d = &g->debris[i];
        if (!d->active) continue;
        d->vel.y -= 22.0f * dt;
        d->pos = gk_vec3_add(d->pos, gk_vec3_scale(d->vel, dt));
        d->pos.z += g->speed * dt * 0.35f;
        d->spin += 6.0f * dt;
        if (d->pos.y < 0.15f && d->vel.y < 0.0f) {
            d->pos.y = 0.15f;
            d->vel.y = -d->vel.y * 0.4f;
            d->vel.x *= 0.8f;
        }
        d->life -= dt;
        if (d->life <= 0.0f || d->pos.z > 10.0f) d->active = false;
    }

    g->invuln = gk_maxf(0.0f, g->invuln - dt);
    g->flash = g->invuln > 0.0f && fmodf(g->invuln, 0.2f) < 0.1f;
}

/* ---- rendering -------------------------------------------------------------- */

static void draw_lit_common(game_t* g, const gk_mat4* vp) {
    gk_shader_use(g->lit);
    gk_shader_set_mat4(g->lit, "u_vp", vp);
    gk_shader_set_vec3(g->lit, "u_light_dir", g->light_dir.x, g->light_dir.y, g->light_dir.z);
    gk_shader_set_vec3(g->lit, "u_light_col", g->light_col.x, g->light_col.y, g->light_col.z);
    gk_shader_set_float(g->lit, "u_ambient", g->ambient);
    gk_shader_set_vec3(g->lit, "u_fog_col", g->fog_col.r, g->fog_col.g, g->fog_col.b);
    gk_shader_set_float(g->lit, "u_fog_near", 30.0f);
    gk_shader_set_float(g->lit, "u_fog_far", 115.0f);
    gk_shader_set_vec2(g->lit, "u_uv_scroll", 0.0f, 0.0f);
}

static void hud_centered(game_t* g, const char* s, float y, float sc, gk_color c) {
    float x = ((float)g->sw - gk_font_width(s, sc)) * 0.5f;
    gk_font_draw(g->hud, s, x, y, sc, c);
}

static void game_render(gk_app* app) {
    game_t* g = (game_t*)app->user;
    gk_camera cam;
    gk_mat4 view, proj, vp, model;
    int i;

    glClearColor(g->fog_col.r, g->fog_col.g, g->fog_col.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    /* camera with crash shake */
    {
        float sx = (gk_rand01() - 0.5f) * g->shake * 0.5f;
        float sy = (gk_rand01() - 0.5f) * g->shake * 0.4f;
        cam.pos    = gk_v3(g->lane_x * 0.35f + sx, 3.4f + sy, 6.6f);
        cam.target = gk_v3(g->lane_x * 0.75f, 1.0f, -8.0f);
        cam.up     = gk_v3(0.0f, 1.0f, 0.0f);
        cam.fov_y  = 58.0f;
        cam.near_z = 0.1f;
        cam.far_z  = 150.0f;
    }
    view = gk_camera_view(&cam);
    proj = gk_camera_proj(&cam, gk_window_aspect(gk_app_window()));
    vp = gk_mat4_mul(proj, view);

    /* --- 3D: road --- */
    draw_lit_common(g, &vp);
    gk_shader_set_vec3(g->lit, "u_cam_pos", cam.pos.x, cam.pos.y, cam.pos.z);
    model = gk_mat4_translate(0.0f, 0.0f, ROAD_ZOFF);
    gk_shader_set_mat4(g->lit, "u_model", &model);
    gk_shader_set_vec2(g->lit, "u_uv_scroll", 0.0f, g->uv_scroll);
    gk_shader_set_vec4(g->lit, "u_tint", 1.0f, 1.0f, 1.0f, 1.0f);
    gk_texture_bind(&g->road_tex, 0);
    gk_shader_set_int(g->lit, "u_tex", 0);
    gk_mesh_draw(&g->road, g->lit);

    /* --- obstacles --- */
    gk_shader_set_vec2(g->lit, "u_uv_scroll", 0.0f, 0.0f);
    gk_texture_bind(&g->crate_tex, 0);
    for (i = 0; i < MAX_OBST; i++) {
        obst_t* o = &g->obst[i];
        if (!o->alive) continue;
        model = gk_mat4_mul(gk_mat4_translate(o->x, 0.8f, o->z),
                            gk_mat4_rot_y(o->spin));
        gk_shader_set_mat4(g->lit, "u_model", &model);
        gk_shader_set_vec4(g->lit, "u_tint", o->tint.r, o->tint.g, o->tint.b, o->tint.a);
        gk_mesh_draw(&g->crate, g->lit);
    }

    /* --- pylons (OBJ model) --- */
    gk_texture_bind(&g->pylon_tex, 0);
    gk_shader_set_vec4(g->lit, "u_tint", 1.0f, 1.0f, 1.0f, 1.0f);
    for (i = 0; i < PYLON_ROWS; i++) {
        float z = g->pylon_z[i];
        model = gk_mat4_translate(-4.6f, 0.0f, z);
        gk_shader_set_mat4(g->lit, "u_model", &model);
        gk_mesh_draw(&g->pylon, g->lit);
        model = gk_mat4_translate(4.6f, 0.0f, z);
        gk_shader_set_mat4(g->lit, "u_model", &model);
        gk_mesh_draw(&g->pylon, g->lit);
    }

    /* --- player ball --- */
    model = gk_mat4_mul(gk_mat4_translate(g->lane_x, 0.55f, 0.0f),
                        gk_mat4_rot_x(g->ball_rot));
    gk_shader_set_mat4(g->lit, "u_model", &model);
    gk_shader_set_vec4(g->lit, "u_tint",
                       g->flash ? 1.0f : 1.0f,
                       g->flash ? 0.35f : 1.0f,
                       g->flash ? 0.35f : 1.0f, 1.0f);
    gk_texture_bind(&g->sphere_tex, 0);
    gk_mesh_draw(&g->ball, g->lit);

    /* --- debris (unlit, alpha-faded) --- */
    gk_shader_use(g->unlit);
    gk_shader_set_mat4(g->unlit, "u_vp", &vp);
    gk_shader_set_vec2(g->unlit, "u_uv_scroll", 0.0f, 0.0f);
    gk_shader_set_int(g->unlit, "u_tex", 0);
    gk_texture_bind(&g->crate_tex, 0);
    glEnable(GL_BLEND);
    for (i = 0; i < MAX_DEBRIS; i++) {
        debris_t* d = &g->debris[i];
        if (!d->active) continue;
        model = gk_mat4_trs(d->pos, d->spin, gk_v3(d->scale, d->scale, d->scale));
        gk_shader_set_mat4(g->unlit, "u_model", &model);
        gk_shader_set_vec4(g->unlit, "u_tint", 1.0f, 0.6f, 0.3f,
                           gk_clampf(d->life, 0.0f, 1.0f));
        gk_mesh_draw(&g->debris_mesh, g->unlit);
    }
    glDisable(GL_BLEND);

    /* --- HUD --- */
    glDisable(GL_DEPTH_TEST);
    gk_sprite_begin_screen(g->hud, g->sw, g->sh);
    {
        const float hs = g->hud_scale;
        const float dim = 0.72f;
        char buf[64];
        int score = (int)g->distance;

        if (g->state == ST_TITLE) {
            hud_centered(g, "GK RUNNER", (float)g->sh * 0.24f, hs * 2.0f, GK_WHITE);
            hud_centered(g, "A 3-LANE GLES2 DODGER", (float)g->sh * 0.24f + 18.0f * hs, hs,
                         gk_col(dim, dim, dim, 1.0f));
            hud_centered(g, "ARROWS OR A/D - DODGE", (float)g->sh * 0.50f, hs, GK_WHITE);
            hud_centered(g, "ENTER - START", (float)g->sh * 0.50f + 11.0f * hs, hs, GK_YELLOW);
            hud_centered(g, "R - RELOAD SHADERS", (float)g->sh * 0.50f + 22.0f * hs, hs,
                         gk_col(dim, dim, dim, 1.0f));
            hud_centered(g, "ESC - QUIT", (float)g->sh * 0.50f + 33.0f * hs, hs,
                         gk_col(dim, dim, dim, 1.0f));
            snprintf(buf, sizeof buf, "HI %06.0f", g->hi);
            hud_centered(g, buf, (float)g->sh * 0.86f, hs, gk_col(1.0f, 0.85f, 0.4f, 1.0f));
        } else {
            snprintf(buf, sizeof buf, "SCORE %06d", score);
            gk_font_draw(g->hud, buf, 16.0f, 14.0f, hs, GK_WHITE);
            snprintf(buf, sizeof buf, "HI %06.0f", g->hi);
            gk_font_draw(g->hud, buf, 16.0f, 14.0f + 10.0f * hs, hs,
                         gk_col(1.0f, 0.85f, 0.4f, 1.0f));

            /* speed bar (top right) */
            {
                float bar_w = 40.0f * hs, bar_h = 5.0f * hs;
                float bx = (float)g->sw - 16.0f - bar_w;
                float by = 18.0f;
                float fill = bar_w * (g->speed / MAX_SPEED);
                gk_sprite_rect(g->hud, bx, by, bar_w, bar_h, gk_col(0.15f, 0.16f, 0.2f, 0.85f));
                gk_sprite_rect(g->hud, bx, by, fill, bar_h,
                               gk_col_lerp(GK_GREEN, GK_RED, g->speed / MAX_SPEED));
                snprintf(buf, sizeof buf, "SPD %02.0f", g->speed);
                gk_font_draw(g->hud, buf, bx, by + bar_h + 4.0f, hs * 0.75f,
                             gk_col(dim, dim, dim, 1.0f));
                for (i = 0; i < g->lives; i++)
                    gk_sprite_rect(g->hud, bx + bar_w - (float)(i + 1) * 10.0f * hs,
                                   by + bar_h + 14.0f * hs, 8.0f * hs, 8.0f * hs, GK_RED);
            }

            if (g->state == ST_OVER) {
                hud_centered(g, "CRASHED!", (float)g->sh * 0.40f, hs * 2.0f, GK_RED);
                snprintf(buf, sizeof buf, "SCORE %06d", score);
                hud_centered(g, buf, (float)g->sh * 0.40f + 20.0f * hs, hs, GK_WHITE);
                if (score >= (int)g->hi && score > 0)
                    hud_centered(g, "NEW RECORD!", (float)g->sh * 0.40f + 32.0f * hs, hs, GK_YELLOW);
                if (fmodf(gk_app_time(), 0.8f) < 0.5f)
                    hud_centered(g, "ENTER - RETRY", (float)g->sh * 0.72f, hs, GK_WHITE);
            }
        }
    }
    gk_sprite_end(g->hud);
    glEnable(GL_DEPTH_TEST);
}

static void game_shutdown(gk_app* app) {
    game_t* g = (game_t*)app->user;
    if (!g) return;
    if (g->lit)   gk_shader_destroy(g->lit);
    if (g->unlit) gk_shader_destroy(g->unlit);
    gk_texture_destroy(&g->road_tex);
    gk_texture_destroy(&g->crate_tex);
    gk_texture_destroy(&g->sphere_tex);
    gk_texture_destroy(&g->pylon_tex);
    gk_mesh_destroy(&g->road);
    gk_mesh_destroy(&g->crate);
    gk_mesh_destroy(&g->ball);
    gk_mesh_destroy(&g->pylon);
    gk_mesh_destroy(&g->debris_mesh);
    gk_sprite_destroy(g->hud);
    gk_sound_destroy(g->s_switch);
    gk_sound_destroy(g->s_crash);
    gk_sound_destroy(g->s_start);
    gk_sound_destroy(g->s_over);
    gk_sound_destroy(g->music);
    free(g);
    app->user = NULL;
}

int main(void) {
    gk_app app;
    memset(&app, 0, sizeof app);
    app.title = "GK Runner";
    app.width = 0;                /* native display resolution */
    app.height = 0;
    app.clear = gk_col(0.55f, 0.72f, 0.92f, 1.0f);
    app.quit_on_esc = true;
    app.use_audio = true;
    app.init = game_init;
    app.update = game_update;
    app.render = game_render;
    app.shutdown = game_shutdown;
    return gk_app_run(&app);
}
