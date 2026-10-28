/* =========================================================================
 * gk_input.c — raw evdev input (keyboard / mouse / gamepad), no Xorg
 *
 * Devices are discovered under /dev/input/event* and classified by their
 * capability bits. Events are consumed non-blocking once per frame.
 * ========================================================================= */
#include "gk.h"
#include "gk_input.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define GK_MAX_DEVICES 16

enum { DEV_KB = 1, DEV_MOUSE = 2, DEV_PAD = 4 };

typedef struct {
    int      fd;
    unsigned kinds;
    char     name[80];
} gk__dev;

typedef struct {
    gk_u8 down, pressed, released;
} gk__btn_state;

static struct {
    gk__dev dev[GK_MAX_DEVICES];
    int     ndev;

    int sw, sh;                    /* screen size for mouse clamping */

    gk__btn_state key[GK_KEY_COUNT];
    gk__btn_state mbtn[GK_MOUSE_COUNT];
    gk__btn_state pbtn[GK_PAD_COUNT];

    float mx, my, mdx, mdy, wheel;
    float axis[4];                 /* LX LY RX RY (merged pads) */
    float trig[2];                 /* LT RT */
    struct input_absinfo ai[16];   /* cached ranges for ABS codes 0..15 */
    bool  have_ai[16];
    int   pad_count;
} IN;

/* ---- linux keycode -> gk_key map ---------------------------------------- */
static const gk_u8 gk__keymap[256] = {
    [KEY_ESC]        = GK_KEY_ESCAPE,
    [KEY_1]          = GK_KEY_1,   [KEY_2] = GK_KEY_2,
    [KEY_3]          = GK_KEY_3,   [KEY_4] = GK_KEY_4,
    [KEY_5]          = GK_KEY_5,   [KEY_6] = GK_KEY_6,
    [KEY_7]          = GK_KEY_7,   [KEY_8] = GK_KEY_8,
    [KEY_9]          = GK_KEY_9,   [KEY_0] = GK_KEY_0,
    [KEY_MINUS]      = GK_KEY_MINUS,
    [KEY_EQUAL]      = GK_KEY_EQUAL,
    [KEY_BACKSPACE]  = GK_KEY_BACKSPACE,
    [KEY_TAB]        = GK_KEY_TAB,
    [KEY_Q]          = GK_KEY_Q,   [KEY_W] = GK_KEY_W,
    [KEY_E]          = GK_KEY_E,   [KEY_R] = GK_KEY_R,
    [KEY_T]          = GK_KEY_T,   [KEY_Y] = GK_KEY_Y,
    [KEY_U]          = GK_KEY_U,   [KEY_I] = GK_KEY_I,
    [KEY_O]          = GK_KEY_O,   [KEY_P] = GK_KEY_P,
    [KEY_LEFTBRACE]  = GK_KEY_LEFT_BRACKET,
    [KEY_RIGHTBRACE] = GK_KEY_RIGHT_BRACKET,
    [KEY_ENTER]      = GK_KEY_RETURN,
    [KEY_LEFTCTRL]   = GK_KEY_LEFT_CTRL,
    [KEY_A]          = GK_KEY_A,   [KEY_S] = GK_KEY_S,
    [KEY_D]          = GK_KEY_D,   [KEY_F] = GK_KEY_F,
    [KEY_G]          = GK_KEY_G,   [KEY_H] = GK_KEY_H,
    [KEY_J]          = GK_KEY_J,   [KEY_K] = GK_KEY_K,
    [KEY_L]          = GK_KEY_L,
    [KEY_SEMICOLON]  = GK_KEY_SEMICOLON,
    [KEY_APOSTROPHE] = GK_KEY_APOSTROPHE,
    [KEY_GRAVE]      = GK_KEY_GRAVE,
    [KEY_LEFTSHIFT]  = GK_KEY_LEFT_SHIFT,
    [KEY_BACKSLASH]  = GK_KEY_BACKSLASH,
    [KEY_Z]          = GK_KEY_Z,   [KEY_X] = GK_KEY_X,
    [KEY_C]          = GK_KEY_C,   [KEY_V] = GK_KEY_V,
    [KEY_B]          = GK_KEY_B,   [KEY_N] = GK_KEY_N,
    [KEY_M]          = GK_KEY_M,
    [KEY_COMMA]      = GK_KEY_COMMA,
    [KEY_DOT]        = GK_KEY_PERIOD,
    [KEY_SLASH]      = GK_KEY_SLASH,
    [KEY_RIGHTSHIFT] = GK_KEY_RIGHT_SHIFT,
    [KEY_LEFTALT]    = GK_KEY_LEFT_ALT,
    [KEY_SPACE]      = GK_KEY_SPACE,
    [KEY_F1]         = GK_KEY_F1,  [KEY_F2]  = GK_KEY_F2,
    [KEY_F3]         = GK_KEY_F3,  [KEY_F4]  = GK_KEY_F4,
    [KEY_F5]         = GK_KEY_F5,  [KEY_F6]  = GK_KEY_F6,
    [KEY_F7]         = GK_KEY_F7,  [KEY_F8]  = GK_KEY_F8,
    [KEY_F9]         = GK_KEY_F9,  [KEY_F10] = GK_KEY_F10,
    [KEY_F11]        = GK_KEY_F11, [KEY_F12] = GK_KEY_F12,
    [KEY_KP7]        = GK_KEY_KP_7, [KEY_KP8] = GK_KEY_KP_8,
    [KEY_KP9]        = GK_KEY_KP_9, [KEY_KPMINUS] = GK_KEY_KP_MINUS,
    [KEY_KP4]        = GK_KEY_KP_4, [KEY_KP5] = GK_KEY_KP_5,
    [KEY_KP6]        = GK_KEY_KP_6, [KEY_KPPLUS]  = GK_KEY_KP_PLUS,
    [KEY_KP1]        = GK_KEY_KP_1, [KEY_KP2] = GK_KEY_KP_2,
    [KEY_KP3]        = GK_KEY_KP_3, [KEY_KP0] = GK_KEY_KP_0,
    [KEY_KPDOT]      = GK_KEY_KP_DOT,
    [KEY_RIGHTCTRL]  = GK_KEY_RIGHT_CTRL,
    [KEY_KPSLASH]    = GK_KEY_KP_DIVIDE,
    [KEY_KPASTERISK] = GK_KEY_KP_MULTIPLY,
    [KEY_RIGHTALT]   = GK_KEY_RIGHT_ALT,
    [KEY_HOME]       = GK_KEY_HOME,
    [KEY_UP]         = GK_KEY_UP,
    [KEY_PAGEUP]     = GK_KEY_PAGE_UP,
    [KEY_LEFT]       = GK_KEY_LEFT,
    [KEY_RIGHT]      = GK_KEY_RIGHT,
    [KEY_END]        = GK_KEY_END,
    [KEY_DOWN]       = GK_KEY_DOWN,
    [KEY_PAGEDOWN]   = GK_KEY_PAGE_DOWN,
    [KEY_INSERT]     = GK_KEY_INSERT,
    [KEY_DELETE]     = GK_KEY_DELETE,
    [KEY_KPENTER]    = GK_KEY_KP_ENTER,
};

/* ---- linux button code -> gk_pad_btn map --------------------------------- */
static const gk_u8 gk__padmap[512] = {
    [BTN_SOUTH]  = GK_PAD_A,
    [BTN_EAST]   = GK_PAD_B,
    [BTN_NORTH]  = GK_PAD_X,
    [BTN_WEST]   = GK_PAD_Y,
    [BTN_TL]     = GK_PAD_L1,
    [BTN_TR]     = GK_PAD_R1,
    [BTN_START]  = GK_PAD_START,
    [BTN_SELECT] = GK_PAD_SELECT,
    [BTN_THUMBL] = GK_PAD_L3,
    [BTN_THUMBR] = GK_PAD_R3,
};

/* ---- helpers -------------------------------------------------------------- */

static void gk__set(gk__btn_state* st, int val) {
    if (val) {
        if (!st->down) { st->down = 1; st->pressed = 1; }
    } else {
        if (st->down) { st->down = 0; st->released = 1; }
    }
}

#define TESTBIT(arr, bit) (((arr)[(bit) >> 3] >> ((bit) & 7)) & 1)

static unsigned gk__classify(int fd, gk_u8* keybits) {
    gk_u8 evbits[4], relbits[4], absbits[16];
    unsigned kinds = 0;

    memset(evbits, 0, sizeof evbits);
    memset(relbits, 0, sizeof relbits);
    memset(absbits, 0, sizeof absbits);

    if (ioctl(fd, EVIOCGBIT(0, sizeof evbits), evbits) < 0) return 0;
    if (!TESTBIT(evbits, EV_KEY)) return 0;

    memset(keybits, 0, 64);
    ioctl(fd, EVIOCGBIT(EV_KEY, 64), keybits);
    ioctl(fd, EVIOCGBIT(EV_REL, sizeof relbits), relbits);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof absbits), absbits);

    if (TESTBIT(keybits, KEY_Q) || TESTBIT(keybits, KEY_1))
        kinds |= DEV_KB;
    if (TESTBIT(relbits, REL_X) && TESTBIT(relbits, REL_Y) &&
        TESTBIT(keybits, BTN_LEFT))
        kinds |= DEV_MOUSE;
    if (TESTBIT(absbits, ABS_X) && TESTBIT(absbits, ABS_Y) &&
        (TESTBIT(keybits, BTN_SOUTH) || TESTBIT(keybits, BTN_TRIGGER) ||
         TESTBIT(keybits, BTN_JOYSTICK)))
        kinds |= DEV_PAD;

    return kinds;
}

static void gk__cache_absinfo(int fd) {
    int code;
    for (code = 0; code < 16; code++) {
        struct input_absinfo ai;
        if (ioctl(fd, EVIOCGABS(code), &ai) == 0) {
            IN.ai[code] = ai;
            IN.have_ai[code] = true;
        }
    }
}

static float gk__axis_norm(int code, int value) {
    const struct input_absinfo* ai = &IN.ai[code];
    float t;
    if (!IN.have_ai[code] || ai->maximum <= ai->minimum) return 0.0f;
    t = 2.0f * ((float)value - (float)ai->minimum) /
        ((float)ai->maximum - (float)ai->minimum) - 1.0f;
    t = gk_clampf(t, -1.0f, 1.0f);
    if (fabsf(t) < 0.22f) return 0.0f;   /* deadzone */
    return t;
}

static float gk__trigger_norm(int code, int value) {
    const struct input_absinfo* ai = &IN.ai[code];
    if (!IN.have_ai[code] || ai->maximum <= ai->minimum) return 0.0f;
    return gk_clampf(((float)value - (float)ai->minimum) /
                     ((float)ai->maximum - (float)ai->minimum), 0.0f, 1.0f);
}

static void gk__remove_device(int i) {
    if (IN.dev[i].fd >= 0) close(IN.dev[i].fd);
    IN.dev[i] = IN.dev[IN.ndev - 1];
    IN.ndev--;
}

static void gk__dispatch(const struct input_event* ev, unsigned kinds) {
    switch (ev->type) {
    case EV_KEY: {
        int val = ev->value ? 1 : 0;
        unsigned code = ev->code;
        if (code < 256 && (kinds & DEV_KB) && gk__keymap[code])
            gk__set(&IN.key[gk__keymap[code]], val);
        if ((kinds & DEV_MOUSE) && code >= BTN_MOUSE && code <= BTN_MIDDLE &&
            (code - BTN_MOUSE) < GK_MOUSE_COUNT)
            gk__set(&IN.mbtn[code - BTN_MOUSE], val);
        if (code < 512 && (kinds & DEV_PAD) && gk__padmap[code])
            gk__set(&IN.pbtn[gk__padmap[code]], val);
        break;
    }
    case EV_REL:
        if (kinds & DEV_MOUSE) {
            switch (ev->code) {
            case REL_X:
                IN.mdx += (float)ev->value;
                IN.mx += (float)ev->value;
                break;
            case REL_Y:
                IN.mdy += (float)ev->value;
                IN.my += (float)ev->value;
                break;
            case REL_WHEEL:
                IN.wheel += (float)ev->value;
                break;
            default:
                break;
            }
        }
        break;
    case EV_ABS:
        if (kinds & DEV_PAD) {
            switch (ev->code) {
            case ABS_X:  IN.axis[0] = gk__axis_norm(0, ev->value);  break;
            case ABS_Y:  IN.axis[1] = gk__axis_norm(1, ev->value);  break;
            case ABS_RX: IN.axis[2] = gk__axis_norm(3, ev->value);  break;
            case ABS_RY: IN.axis[3] = gk__axis_norm(4, ev->value);  break;
            case ABS_Z: case ABS_BRAKE:
                IN.trig[0] = gk__trigger_norm(ev->code == ABS_Z ? 2 : 10, ev->value);
                break;
            case ABS_RZ: case ABS_GAS:
                IN.trig[1] = gk__trigger_norm(ev->code == ABS_RZ ? 5 : 9, ev->value);
                break;
            case ABS_HAT0X:
                gk__set(&IN.pbtn[GK_PAD_LEFT],  ev->value == -1);
                gk__set(&IN.pbtn[GK_PAD_RIGHT], ev->value ==  1);
                break;
            case ABS_HAT0Y:
                gk__set(&IN.pbtn[GK_PAD_UP],   ev->value == -1);
                gk__set(&IN.pbtn[GK_PAD_DOWN], ev->value ==  1);
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
}

/* ---- public API ------------------------------------------------------------ */

int gk_input_init(void) {
    DIR* dir;
    struct dirent* de;
    IN.ndev = 0;
    IN.sw = 640;
    IN.sh = 480;
    IN.mx = 320.0f;
    IN.my = 240.0f;
    IN.pad_count = 0;

    dir = opendir("/dev/input");
    if (!dir) {
        GK_WARN("cannot open /dev/input\n");
        return 0;
    }
    while ((de = readdir(dir)) != NULL && IN.ndev < GK_MAX_DEVICES) {
        char path[280];
        gk_u8 keybits[64];
        unsigned kinds;
        int fd;
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        snprintf(path, sizeof path, "/dev/input/%.64s", de->d_name);
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        kinds = gk__classify(fd, keybits);
        if (!kinds) {
            close(fd);
            continue;
        }
        IN.dev[IN.ndev].fd = fd;
        IN.dev[IN.ndev].kinds = kinds;
        IN.dev[IN.ndev].name[0] = 0;
        ioctl(fd, EVIOCGNAME(sizeof IN.dev[0].name) - 1, IN.dev[IN.ndev].name);
        if (kinds & DEV_PAD) {
            gk__cache_absinfo(fd);
            IN.pad_count++;
        }
        GK_LOG("input: %s [%s %s %s]\n", path,
               (kinds & DEV_KB)    ? "kb"    : "",
               (kinds & DEV_MOUSE) ? "mouse" : "",
               (kinds & DEV_PAD)   ? "pad"   : "");
        IN.ndev++;
    }
    closedir(dir);

    if (IN.ndev == 0)
        GK_WARN("no usable input devices (are you in the 'input' group?)\n");
    return IN.ndev;
}

void gk_input_poll(void) {
    int i;
    for (i = 0; i < GK_KEY_COUNT; i++)   { IN.key[i].pressed = 0;  IN.key[i].released = 0; }
    for (i = 0; i < GK_MOUSE_COUNT; i++) { IN.mbtn[i].pressed = 0; IN.mbtn[i].released = 0; }
    for (i = 0; i < GK_PAD_COUNT; i++)   { IN.pbtn[i].pressed = 0; IN.pbtn[i].released = 0; }
    IN.mdx = 0.0f;
    IN.mdy = 0.0f;
    IN.wheel = 0.0f;

    for (i = 0; i < IN.ndev; ) {
        struct input_event ev;
        long n;
        bool alive = true;
        while ((n = (long)read(IN.dev[i].fd, &ev, sizeof ev)) == (long)sizeof ev)
            gk__dispatch(&ev, IN.dev[i].kinds);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                alive = false;
        } else {
            alive = false;   /* partial read: stream is broken */
        }
        if (alive) {
            i++;
        } else {
            GK_WARN("input device %d lost\n", i);
            gk__remove_device(i);
        }
    }

    if (IN.sw > 0) IN.mx = gk_clampf(IN.mx, 0.0f, (float)IN.sw - 1.0f);
    if (IN.sh > 0) IN.my = gk_clampf(IN.my, 0.0f, (float)IN.sh - 1.0f);
}

void gk_input_set_screen_size(int w, int h) {
    IN.sw = w;
    IN.sh = h;
    if (IN.mx == 0.0f && IN.my == 0.0f) {
        IN.mx = w * 0.5f;
        IN.my = h * 0.5f;
    }
}

int gk_input_device_count(void) { return IN.ndev; }

bool gk_key_down(gk_key k)     { return (k > 0 && k < GK_KEY_COUNT) ? IN.key[k].down     != 0 : false; }
bool gk_key_pressed(gk_key k)  { return (k > 0 && k < GK_KEY_COUNT) ? IN.key[k].pressed  != 0 : false; }
bool gk_key_released(gk_key k) { return (k > 0 && k < GK_KEY_COUNT) ? IN.key[k].released != 0 : false; }

void gk_mouse_pos(float* x, float* y) {
    if (x) *x = IN.mx;
    if (y) *y = IN.my;
}
void gk_mouse_delta(float* dx, float* dy) {
    if (dx) *dx = IN.mdx;
    if (dy) *dy = IN.mdy;
}
bool gk_mouse_down(gk_mouse_btn b)     { return (b >= 0 && b < GK_MOUSE_COUNT) ? IN.mbtn[b].down     != 0 : false; }
bool gk_mouse_pressed(gk_mouse_btn b)  { return (b >= 0 && b < GK_MOUSE_COUNT) ? IN.mbtn[b].pressed  != 0 : false; }
bool gk_mouse_released(gk_mouse_btn b) { return (b >= 0 && b < GK_MOUSE_COUNT) ? IN.mbtn[b].released != 0 : false; }
float gk_mouse_wheel(void) { return IN.wheel; }

bool gk_pad_down(int pad, gk_pad_btn b)     { (void)pad; return (b >= 0 && b < GK_PAD_COUNT) ? IN.pbtn[b].down     != 0 : false; }
bool gk_pad_pressed(int pad, gk_pad_btn b)  { (void)pad; return (b >= 0 && b < GK_PAD_COUNT) ? IN.pbtn[b].pressed  != 0 : false; }
bool gk_pad_released(int pad, gk_pad_btn b) { (void)pad; return (b >= 0 && b < GK_PAD_COUNT) ? IN.pbtn[b].released != 0 : false; }
float gk_pad_axis(int pad, int axis)        { (void)pad; return (axis >= 0 && axis < 4) ? IN.axis[axis] : 0.0f; }
float gk_pad_trigger(int pad, int side)     { (void)pad; return (side == 0 || side == 1) ? IN.trig[side] : 0.0f; }
int gk_pad_count(void)                      { return IN.pad_count; }

void gk_input_shutdown(void) {
    int i;
    for (i = 0; i < IN.ndev; i++)
        if (IN.dev[i].fd >= 0) close(IN.dev[i].fd);
    IN.ndev = 0;
}
