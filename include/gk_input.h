/* =========================================================================
 * gk_input.h — evdev input: keyboard, mouse, gamepads (/dev/input/event*)
 *
 * No Xorg: devices are read straight from the Linux input subsystem.
 * Requires read permission on /dev/input/event* (add yourself to the
 * "input" group, or run as root).
 *
 * All connected gamepads are merged into pad 0.
 * ========================================================================= */
#ifndef GK_INPUT_H
#define GK_INPUT_H

#include "gk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- keyboard ----------------------------------------------------------- */
typedef enum {
    GK_KEY_UNKNOWN = 0,
    GK_KEY_A, GK_KEY_B, GK_KEY_C, GK_KEY_D, GK_KEY_E, GK_KEY_F,
    GK_KEY_G, GK_KEY_H, GK_KEY_I, GK_KEY_J, GK_KEY_K, GK_KEY_L,
    GK_KEY_M, GK_KEY_N, GK_KEY_O, GK_KEY_P, GK_KEY_Q, GK_KEY_R,
    GK_KEY_S, GK_KEY_T, GK_KEY_U, GK_KEY_V, GK_KEY_W, GK_KEY_X,
    GK_KEY_Y, GK_KEY_Z,
    GK_KEY_0, GK_KEY_1, GK_KEY_2, GK_KEY_3, GK_KEY_4,
    GK_KEY_5, GK_KEY_6, GK_KEY_7, GK_KEY_8, GK_KEY_9,
    GK_KEY_RETURN, GK_KEY_ESCAPE, GK_KEY_BACKSPACE, GK_KEY_TAB, GK_KEY_SPACE,
    GK_KEY_UP, GK_KEY_DOWN, GK_KEY_LEFT, GK_KEY_RIGHT,
    GK_KEY_LEFT_SHIFT, GK_KEY_RIGHT_SHIFT,
    GK_KEY_LEFT_CTRL, GK_KEY_RIGHT_CTRL,
    GK_KEY_LEFT_ALT, GK_KEY_RIGHT_ALT,
    GK_KEY_MINUS, GK_KEY_EQUAL,
    GK_KEY_LEFT_BRACKET, GK_KEY_RIGHT_BRACKET, GK_KEY_BACKSLASH,
    GK_KEY_SEMICOLON, GK_KEY_APOSTROPHE, GK_KEY_GRAVE,
    GK_KEY_COMMA, GK_KEY_PERIOD, GK_KEY_SLASH,
    GK_KEY_F1, GK_KEY_F2, GK_KEY_F3, GK_KEY_F4, GK_KEY_F5, GK_KEY_F6,
    GK_KEY_F7, GK_KEY_F8, GK_KEY_F9, GK_KEY_F10, GK_KEY_F11, GK_KEY_F12,
    GK_KEY_HOME, GK_KEY_END, GK_KEY_PAGE_UP, GK_KEY_PAGE_DOWN,
    GK_KEY_INSERT, GK_KEY_DELETE,
    GK_KEY_KP_0, GK_KEY_KP_1, GK_KEY_KP_2, GK_KEY_KP_3, GK_KEY_KP_4,
    GK_KEY_KP_5, GK_KEY_KP_6, GK_KEY_KP_7, GK_KEY_KP_8, GK_KEY_KP_9,
    GK_KEY_KP_DIVIDE, GK_KEY_KP_MULTIPLY, GK_KEY_KP_MINUS,
    GK_KEY_KP_PLUS, GK_KEY_KP_ENTER, GK_KEY_KP_DOT,
    GK_KEY_COUNT
} gk_key;

bool gk_key_down(gk_key key);
bool gk_key_pressed(gk_key key);     /* went down this frame */
bool gk_key_released(gk_key key);    /* came up this frame   */

/* ---- mouse --------------------------------------------------------------- */
typedef enum { GK_MOUSE_LEFT = 0, GK_MOUSE_RIGHT, GK_MOUSE_MIDDLE,
               GK_MOUSE_COUNT } gk_mouse_btn;

void gk_mouse_pos(float* x, float* y);     /* screen px, clamped */
void gk_mouse_delta(float* dx, float* dy); /* accumulated this frame */
bool gk_mouse_down(gk_mouse_btn b);
bool gk_mouse_pressed(gk_mouse_btn b);
bool gk_mouse_released(gk_mouse_btn b);
float gk_mouse_wheel(void);                /* accumulated this frame, +1 per notch up */

/* ---- gamepad -------------------------------------------------------------- */
typedef enum {
    GK_PAD_A, GK_PAD_B, GK_PAD_X, GK_PAD_Y,
    GK_PAD_L1, GK_PAD_R1,
    GK_PAD_START, GK_PAD_SELECT,
    GK_PAD_UP, GK_PAD_DOWN, GK_PAD_LEFT, GK_PAD_RIGHT,
    GK_PAD_L3, GK_PAD_R3,
    GK_PAD_COUNT
} gk_pad_btn;

bool gk_pad_down(int pad, gk_pad_btn b);
bool gk_pad_pressed(int pad, gk_pad_btn b);
bool gk_pad_released(int pad, gk_pad_btn b);
/* axis: 0 = LX, 1 = LY, 2 = RX, 3 = RY  (range -1..1, deadzone applied) */
float gk_pad_axis(int pad, int axis);
/* side: 0 = left trigger, 1 = right trigger (range 0..1) */
float gk_pad_trigger(int pad, int side);
int   gk_pad_count(void);                  /* pads merged into 0 */

/* ---- lifecycle ------------------------------------------------------------ */
int  gk_input_init(void);                  /* returns #devices opened */
void gk_input_poll(void);                  /* once per frame, before update */
void gk_input_set_screen_size(int w, int h);
int  gk_input_device_count(void);
void gk_input_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* GK_INPUT_H */
