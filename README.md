# GameKit — a GLES2 game framework for Raspberry Pi (no Xorg)

A small, dependency-light game framework written in C99, built directly on
the Raspberry Pi's **legacy VideoCore stack**: EGL + GLES2 + Dispmanx, the
same libraries behind `/opt/vc/src/hello_pi`. It targets the Pi 1 / Zero
(BCM2835, ARM1176JZF-S, VideoCore IV) running **Raspbian Buster** without
Xorg — your app becomes a Dispmanx layer over the console.

Everything is shader-driven: meshes, sprites and HUD all go through GLSL
programs that live in `assets/shaders/` and can be edited (and hot-reloaded)
while the game runs.

```
gamekit/
├── Makefile               make / make run / make CROSS=... / make BRCM=1
├── include/               public API (one header per module)
│   ├── gk.h               common types, logging, file utils
│   ├── gk_math.h          vec2/3, colors, mat4 (column-major), camera
│   ├── gk_window.h        EGL + Dispmanx fullscreen display
│   ├── gk_shader.h        program loading + uniform/attribute caches
│   ├── gk_texture.h       RGBA8 textures (PNG/JPEG/BMP/TGA via stb_image)
│   ├── gk_mesh.h          VBO/IBO meshes, primitives, OBJ loader
│   ├── gk_sprite.h        batched 2D quads (HUD, particles)
│   ├── gk_font.h          built-in 8x8 bitmap font
│   ├── gk_input.h         evdev keyboard / mouse / gamepad
│   ├── gk_audio.h         ALSA software mixer (PCM WAV)
│   ├── gk_assets.h        asset root resolution
│   ├── gk_app.h           application lifecycle + main loop
│   └── external/          vendored stb_image.h, font8x8_basic.h (public domain)
├── src/                   implementations (built into libgk.a)
├── assets/
│   ├── shaders/           lit, unlit, flat, sprite (GLSL ES 1.00)
│   ├── images/            road.png, crate.png, sphere.png, pylon.png
│   ├── sounds/            WAV sfx + music loop (generated chiptune)
│   └── models/            pylon.obj
└── examples/
    └── demo/main.c        "GK Runner" — playable 3-lane endless dodger
```

## Requirements (on the Pi)

Raspbian Buster with the default (legacy) graphics stack — the one the
`hello_pi` samples use. No Xorg needed.

```sh
sudo apt update
sudo apt install build-essential libasound2-dev
```

Allow your user to talk to the hardware (then log out/in):

```sh
sudo usermod -aG video,input,audio $USER
```

## Build & run

On the Pi:

```sh
make            # builds build/libgk.a and build/demo
./build/demo    # or: make run
```

Cross-building from a PC:

```sh
make CROSS=arm-linux-gnueabihf-
```

If your `/opt/vc/lib` only has `libbrcmEGL.so` / `libbrcmGLESv2.so`:

```sh
make BRCM=1
```

The demo renders fullscreen at the display's native resolution. Run it from
the Pi's console or via SSH — the picture appears on the physical display.

### Audio routing

The mixer plays through ALSA's `default` device. To send it to the 3.5 mm
jack instead of HDMI:

```sh
amixer cset numid=3 1     # 0=auto, 1=jack, 2=hdmi
```

## Using the framework

```c
#include "gk.h"
#include "gk_app.h"
#include "gk_sprite.h"
#include "gk_font.h"

static gk_sprite* hud;

static bool init(gk_app* app) {
    hud = gk_sprite_create(1024);
    return true;
}

static void update(gk_app* app, float dt) {
    if (gk_key_pressed(GK_KEY_SPACE)) { /* ... */ }
}

static void render(gk_app* app) {
    gk_sprite_begin_screen(hud, gk_window_width(gk_app_window()),
                                 gk_window_height(gk_app_window()));
    gk_font_draw(hud, "hello pi", 16, 16, 4, GK_WHITE);
    gk_sprite_end(hud);
}

int main(void) {
    gk_app app = {0};
    app.title = "hello"; app.quit_on_esc = true; app.use_audio = true;
    app.init = init; app.update = update; app.render = render;
    return gk_app_run(&app);
}
```

Compile with `-Iinclude`, link `libgk.a` plus
`-lEGL -lGLESv2 -lbcm_host -lasound -lm -lpthread` (see the Makefile).

## Modules

| Header | What it gives you |
|---|---|
| `gk_app.h` | Main loop with delta time, vsync, SIGINT/SIGTERM handling, ESC-to-quit |
| `gk_window.h` | Fullscreen EGL/Dispmanx surface; pass 0,0 for native size or a custom render resolution (Dispmanx upscales) |
| `gk_math.h` | `gk_vec3`, `gk_color`, column-major `gk_mat4`, perspective/look-at/ortho cameras |
| `gk_shader.h` | `gk_shader_load("shaders/lit")` → `<path>.vert`+`.frag`; cached uniform lookup by name |
| `gk_texture.h` | PNG/JPEG/BMP/TGA → RGBA8 GPU texture; NEAREST/LINEAR, clamp/repeat |
| `gk_mesh.h` | Cube/plane/sphere primitives, OBJ loader (`v/vt/vn`, triangulated), attribute auto-binding |
| `gk_sprite.h` | Batched textured quads: one draw call per texture change, rotation/flip/tint |
| `gk_font.h` | 8x8 monospace bitmap font drawn through the sprite batch |
| `gk_input.h` | Keyboard map, mouse position/delta/wheel, merged gamepads with axes + triggers |
| `gk_audio.h` | 32-voice mixer thread: `gk_sound_play(snd, vol, pan, loop)`, voice stealing, master volume |
| `gk_assets.h` | Finds `assets/` next to the executable (or `$GK_ASSETS`) |

## Shader contract

All GLSL lives in `assets/shaders/` — edit freely and press **R** in the
demo to hot-reload. Vertex formats the framework binds:

* **meshes** (`gk_mesh_draw`) — attributes `a_pos` (vec3), `a_nrm` (vec3),
  `a_uv` (vec2), interleaved 32 bytes. Uniforms are yours: `u_vp`, `u_model`,
  whatever else the program declares.
* **sprite batch** — attributes `a_pos` (vec2), `a_uv` (vec2), `a_col`
  (vec4, normalized bytes); uniforms `u_mvp`, `u_tex`. The batch loads
  `shaders/sprite` when present, otherwise an embedded default.

Shipped programs:

* `lit` — texture x tint x (ambient + directional diffuse), depth fog
  (`u_fog_col`, `u_fog_near`, `u_fog_far`, `u_cam_pos`) and `u_uv_scroll`
  for scrolling UVs.
* `unlit` — texture x tint (+ `u_uv_scroll`).
* `flat` — solid `u_color`, for lines and overlays.
* `sprite` — textured quads with per-vertex color.

## Asset conventions

* Paths are relative to `assets/`: `"images/road.png"`, `"sounds/blip.wav"`,
  `"shaders/lit"`, `"models/pylon.obj"`.
* **UV rule: v = 0 is the top row of the image** (y-down, like screen
  coordinates). The OBJ loader and primitive generators follow it.
* WAV: PCM, 8/16-bit, mono/stereo, any common rate — converted on load to
  44.1 kHz stereo s16. Looping is free (`loop = true`).
* NPOT textures are fine (VideoCore IV GLES2 handles them with clamp wrap;
  repeat wrap needs power-of-two).

## Performance notes (Pi 1 / Zero)

* The ARM11 has VFPv2 but **no NEON** — keep per-frame CPU work lean; the
  sprite batch and mesh VBOs exist precisely to push work to the VideoCore.
* One draw call per texture change: pack sprites into atlases when you can.
* Fill rate, not triangles, is the usual limit at 1920x1080. Render at a
  lower resolution (e.g. `gk_window_create(&win, 960, 540)`) and let
  Dispmanx upscale — it is nearly free.
* `eglSwapInterval(1)` is set: you get vsync-paced frames (~60 or ~50 Hz).
* `gpu_mem=128` (or more) in `config.txt` is comfortable for 1080p surfaces.

## Troubleshooting

* **`vc_dispmanx_element_add failed`** — your user lacks video permission
  (`sudo usermod -aG video $USER`) or the KMS/fkms overlay replaced the
  legacy stack in `config.txt` (remove `vc4.kms`/`fkms-v3d` entries for the
  legacy Dispmanx path).
* **No input** — check `ls /dev/input/event*` and your `input` group
  membership. Devices are opened at startup; hot-plugging a keyboard later
  is not detected (restart the app).
* **Silent audio** — `gk_audio_ready()` returns false if ALSA could not
  open `default`. Check `aplay -l`, the `audio` group, and the HDMI/jack
  routing note above.
* **Link errors about `brcm` symbols** — build with `make BRCM=1`.
* **Shaders fail to compile** — the error log (with line info) is printed
  to stderr; GLSL ES 1.00 only: no `#version` directive, `--`-style
  comments are not valid, use `//`.

## Licenses

* Framework code: MIT.
* `include/external/stb_image.h` — public domain (by Sean Barrett).
* `include/external/font8x8_basic.h` — public domain (Daniel Hepper,
  Marcel Sondaar, IBM VGA font).
* Generated assets in `assets/` — CC0 (created by `scripts/make_assets.py`).
