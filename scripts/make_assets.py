#!/usr/bin/env python3
"""GameKit asset generator: textures, sounds, model + font verification PNG.
Generates everything under download/gamekit/assets/."""
import zlib, struct, math, random, wave, os, re

ROOT = "/home/z/my-project/download/gamekit/assets"
random.seed(7)

# ---------------------------------------------------------------- PNG writer
def write_png(path, w, h, rgba):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c
    raw = b"".join(b"\x00" + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)

class Img:
    def __init__(self, w, h, base=(0, 0, 0, 255)):
        self.w, self.h = w, h
        self.px = bytearray(base) * (w * h) if len(base) == 4 else bytearray(base * [0, 0, 0, 255])
        if len(base) == 3:
            self.px = bytearray()
            for _ in range(w * h):
                self.px += bytes(base) + b"\xff"
    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            if len(c) == 3:
                c = (c[0], c[1], c[2], 255)
            i = (y * self.w + x) * 4
            self.px[i] = max(0, min(255, int(c[0])))
            self.px[i+1] = max(0, min(255, int(c[1])))
            self.px[i+2] = max(0, min(255, int(c[2])))
            self.px[i+3] = max(0, min(255, int(c[3])))
    def get(self, x, y):
        i = (y * self.w + x) * 4
        return tuple(self.px[i:i+3])
    def save(self, path):
        write_png(path, self.w, self.h, self.px)

# ---------------------------------------------------------------- road.png (128x256 = 8m x 4m, 16 px/m)
road = Img(128, 256, (44, 46, 52, 255))
for y in range(256):
    for x in range(128):
        n = random.randint(-7, 7)
        r, g, b = road.get(x, y)
        road.set(x, y, (r + n, g + n, b + n, 255))
# edge lines (solid white) at +-3.6 m
for x in list(range(3, 8)) + list(range(120, 125)):
    for y in range(256):
        road.set(x, y, (215, 215, 208, 255))
# lane dividers (dashed 2m on / 2m off) at +-1.1 m
for cx in (44, 45, 82, 83):
    for y in range(0, 128):
        road.set(cx, y, (200, 200, 190, 255))
road.save(f"{ROOT}/images/road.png")

# ---------------------------------------------------------------- crate.png 64x64
crate = Img(64, 64, (150, 101, 54, 255))
for y in range(64):
    for x in range(64):
        n = random.randint(-5, 5)
        r, g, b = crate.get(x, y)
        crate.set(x, y, (r + n, g + n, b + n, 255))
for y in range(64):                      # plank gaps
    if y % 16 < 2:
        for x in range(64):
            crate.set(x, y, (112, 74, 38, 255))
def thick_line(img, x0, y0, x1, y1, c, w=4):
    steps = max(abs(x1 - x0), abs(y1 - y0)) * 2
    for s in range(steps + 1):
        t = s / steps
        x = x0 + (x1 - x0) * t
        y = y0 + (y1 - y0) * t
        for dx in range(w):
            for dy in range(w):
                img.set(int(x) + dx, int(y) + dy, c)
for i in range(64):                      # frame
    for j in list(range(5)) + list(range(59, 64)):
        crate.set(i, j, (104, 68, 34, 255)); crate.set(j, i, (104, 68, 34, 255))
thick_line(crate, 5, 5, 57, 57, (118, 78, 40))    # diagonal brace
thick_line(crate, 57, 5, 5, 57, (118, 78, 40))
for (nx, ny) in ((9, 9), (52, 9), (9, 52), (52, 52)):   # nails
    for dx in range(3):
        for dy in range(3):
            crate.set(nx + dx, ny + dy, (70, 46, 24, 255))
crate.save(f"{ROOT}/images/crate.png")

# ---------------------------------------------------------------- sphere.png 64x64 (checker + equator stripe)
sph = Img(64, 64)
for y in range(64):
    for x in range(64):
        if 30 <= y <= 33:
            sph.set(x, y, (206, 64, 52, 255))
        elif ((x // 8) + (y // 8)) % 2 == 0:
            sph.set(x, y, (38, 170, 180, 255))
        else:
            sph.set(x, y, (224, 240, 244, 255))
sph.save(f"{ROOT}/images/sphere.png")

# ---------------------------------------------------------------- pylon.png 32x32
pyl = Img(32, 32, (235, 120, 25, 255))
for y in range(32):
    for x in range(32):
        if 12 <= y <= 19:
            pyl.set(x, y, (240, 238, 230, 255))
        elif y >= 28:
            pyl.set(x, y, (60, 60, 64, 255))
        if x < 5 or x > 26:
            r, g, b = pyl.get(x, y)
            pyl.set(x, y, (r * 0.82, g * 0.82, b * 0.82, 255))
pyl.save(f"{ROOT}/images/pylon.png")

# ---------------------------------------------------------------- pylon.obj (exploded, flat normals)
# cone: base 1.0 at y=0, apex at y=1.8; v=0 apex (orange), v=1 base (dark)
A, B, C, D, E = (-0.5, 0, 0.5), (0.5, 0, 0.5), (0.5, 0, -0.5), (-0.5, 0, -0.5), (0, 1.8, 0)
def tri(p0, p1, p2, n, uv0, uv1, uv2, out):
    for p, uv in zip((p0, p1, p2), (uv0, uv1, uv2)):
        out.append(f"v {p[0]} {p[1]} {p[2]}")
        out.append(f"vt {uv[0]} {uv[1]}")
        out.append(f"vn {n[0]} {n[1]} {n[2]}")
lines = []
# south face (+z): A B E ; north (-z): D C E ; west (+x): B C E ; east (-x): D A E
tri(A, B, E, (0, 0, 1),  (0, 1), (1, 1), (0.5, 0), lines)
tri(D, C, E, (0, 0, -1), (0, 1), (1, 1), (0.5, 0), lines)
tri(B, C, E, (1, 0, 0),  (0, 1), (1, 1), (0.5, 0), lines)
tri(C, D, E, (-1, 0, 0), (0, 1), (1, 1), (0.5, 0), lines)
tri(A, D, C, (0, -1, 0), (0, 1), (0, 1), (1, 1), lines)   # base (2 tris)
tri(A, C, B, (0, -1, 0), (0, 1), (1, 1), (1, 1), lines)
with open(f"{ROOT}/models/pylon.obj", "w") as f:
    f.write("# GameKit pylon (generated)\n")
    for t in range(0, len(lines), 9):          # tri = 9 lines: 3 corners x (v, vt, vn)
        for k in range(9):
            f.write(lines[t + k] + "\n")
        c = t // 3 + 1
        f.write(f"f {c}/{c}/{c} {c+1}/{c+1}/{c+1} {c+2}/{c+2}/{c+2}\n")

# ---------------------------------------------------------------- sounds
RATE = 44100
def save_wav(path, samples):   # samples: list of (l, r) floats
    with wave.open(path, "w") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for l, r in samples:
            l = max(-1.0, min(1.0, l)); r = max(-1.0, min(1.0, r))
            frames += struct.pack("<hh", int(l * 32767), int(r * 32767))
        w.writeframes(bytes(frames))

def square(f, t, duty=0.5):
    ph = (t * f) % 1.0
    return 1.0 if ph < duty else -1.0

def sine(f, t):
    return math.sin(2 * math.pi * f * t)

def env_ad(t, dur, att=0.004, curve=5.0):
    if t < att:
        return t / att
    return math.exp(-curve * (t - att) / max(dur - att, 1e-6))

# switch.wav — quick upward blip
dur = 0.09
sw = []
for i in range(int(dur * RATE)):
    t = i / RATE
    f = 480 + 4200 * t
    v = 0.5 * env_ad(t, dur, 0.002, 6) * sine(f, t)
    sw.append((v, v))
save_wav(f"{ROOT}/sounds/switch.wav", sw)

# crash.wav — noise burst + low thump
dur = 0.45
cr = []
lp = 0.0
for i in range(int(dur * RATE)):
    t = i / RATE
    noise = random.uniform(-1, 1)
    lp += 0.35 * (noise - lp)               # darken the noise
    v = 0.85 * lp * math.exp(-t * 8.5)
    v += 0.7 * sine(62, t) * math.exp(-t * 14)
    v = math.tanh(v * 1.6)
    cr.append((v, v * 0.92))
save_wav(f"{ROOT}/sounds/crash.wav", cr)

# start.wav — rising C5 E5 G5
notes = [(523.25, 0.12), (659.26, 0.12), (783.99, 0.20)]
st = []
for f, nd in notes:
    for i in range(int(nd * RATE)):
        t = i / RATE
        v = 0.34 * env_ad(t, nd, 0.004, 4) * square(f, t)
        st.append((v, v))
st += [(0.0, 0.0)] * int(0.02 * RATE)
save_wav(f"{ROOT}/sounds/start.wav", st)

# over.wav — descending sad phrase
notes = [(440.0, 0.16), (349.23, 0.16), (293.66, 0.20), (246.94, 0.34)]
ov = []
for f, nd in notes:
    for i in range(int(nd * RATE)):
        t = i / RATE
        v = 0.32 * env_ad(t, nd, 0.005, 3.2) * square(f, t, 0.25)
        ov.append((v, v))
save_wav(f"{ROOT}/sounds/over.wav", ov)

# music.wav — 4 bars @126 BPM: Am F C G, bass + arp lead + hats
bpm = 126.0
beat = 60.0 / bpm
bars = [
    (110.00, 164.81, [440.0, 523.25, 659.26, 783.99]),   # Am: A2, E3, [A4 C5 E5 G5]
    (87.31, 130.81, [440.0, 523.25, 698.46, 880.0]),     # F : F2, C3, [A4 C5 F5 A5]
    (130.81, 196.00, [392.0, 523.25, 659.26, 783.99]),   # C : C3, G3, [G4 C5 E5 G5]
    (98.00, 146.83, [392.0, 493.88, 587.33, 783.99]),    # G : G2, D3, [G4 B4 D5 G5]
]
total = int(4 * 4 * beat * RATE)
mus = [[0.0, 0.0] for _ in range(total)]
def add(i, l, r):
    if 0 <= i < total:
        mus[i][0] += l
        mus[i][1] += r
for bar, (broot, bfifth, arp) in enumerate(bars):
    bt0 = bar * 4 * beat
    for e in range(8):                                   # bass 8ths
        f = broot if e % 4 != 2 else bfifth
        nd = beat * 0.5
        for i in range(int(nd * RATE)):
            t = i / RATE
            t0 = e * beat * 0.5
            v = 0.24 * env_ad(t, nd, 0.004, 4) * square(f, t, 0.5)
            add(int((bt0 + t0 + t) * RATE), v * 0.95, v)
    for s16 in range(16):                                # lead 16th arp
        f = arp[s16 % 4]
        nd = beat * 0.25
        if s16 % 2 == 1:                                 # offbeat only
            for i in range(int(nd * RATE)):
                t = i / RATE
                t0 = s16 * beat * 0.25
                v = 0.15 * env_ad(t, nd, 0.003, 5) * sine(f, t)
                add(int((bt0 + t0 + t) * RATE), v * 0.8, v)
    for e in range(4):                                   # hats
        nd = 0.03
        for i in range(int(nd * RATE)):
            t = i / RATE
            t0 = e * beat
            v = (0.10 if e == 0 else 0.055) * math.exp(-t * 160) * random.uniform(-1, 1)
            add(int((bt0 + t0 + t) * RATE), v, v)
peak = max(max(abs(l), abs(r)) for l, r in mus) or 1.0
mus = [((l / peak) * 0.8, (r / peak) * 0.8) for l, r in mus]
save_wav(f"{ROOT}/sounds/music.wav", mus)

# ---------------------------------------------------------------- font verification PNG
src = open("/home/z/my-project/download/gamekit/include/external/font8x8_basic.h").read()
glyphs = re.findall(r"\{\s*(0x[0-9A-Fa-f]{2}(?:,\s*0x[0-9A-Fa-f]{2}){7})\s*\}", src)
gbytes = [[int(x, 16) for x in g.split(", ")] for g in glyphs]
text = "GK RUNNER 0123ah@"
S = 4
W, H = 8 * len(text) * S + 16, 10 * S + 8
img = Img(W, H, (20, 24, 34, 255))
for ci, ch in enumerate(text):
    rows = gbytes[ord(ch) & 0x7F]
    for r in range(8):
        for c in range(8):
            if (rows[r] >> c) & 1:           # LSB = leftmost (verified)
                for dx in range(S):
                    for dy in range(S):
                        img.set(8 + ci * 8 * S + c * S + dx, 4 + r * S + dy, (240, 240, 235, 255))
img.save("/home/z/my-project/scripts/font_test.png")

print("assets generated:")
for dp, _, fn in os.walk(ROOT):
    for f in sorted(fn):
        p = os.path.join(dp, f)
        print(f"  {p.replace(ROOT + '/', ''):24s} {os.path.getsize(p):>9,} bytes")
