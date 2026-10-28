/* =========================================================================
 * gk_audio.c — ALSA playback + software mixer (background thread)
 * ========================================================================= */
#include "gk.h"
#include "gk_audio.h"
#include "gk_assets.h"

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <unistd.h>

#define GK_MIX_RATE   44100
#define GK_MIX_PERIOD 1024

struct gk_sound {
    gk_i16* data;      /* interleaved stereo s16 @ 44100 Hz */
    int     frames;
    float   dur;
};

typedef struct {
    const gk_sound* snd;
    int   pos;         /* current frame */
    float vol;
    float pan;
    bool  loop;
    bool  active;
    gk_u32 stamp;
} gk__voice;

static struct {
    snd_pcm_t* pcm;
    pthread_t  thread;
    bool       running;
    bool       ok;
    float      master;
    gk__voice  ch[GK_AUDIO_MAX_CHANNELS];
    pthread_mutex_t lock;
    gk_u32     play_counter;
} AU;

/* ---- WAV loading ---------------------------------------------------------- */

static gk_u32 gk__u32le(const unsigned char* p) {
    return (gk_u32)p[0] | ((gk_u32)p[1] << 8) |
           ((gk_u32)p[2] << 16) | ((gk_u32)p[3] << 24);
}
static gk_u16 gk__u16le(const unsigned char* p) {
    return (gk_u16)((gk_u16)p[0] | ((gk_u16)p[1] << 8));
}

static float gk__wav_sample(const unsigned char* d, size_t sz, int bits,
                            int ch, int frame, int c) {
    if (frame < 0) frame = 0;
    if (bits == 16) {
        size_t off = ((size_t)frame * ch + c) * 2u;
        if (off + 1 >= sz) return 0.0f;
        return (float)(short)gk__u16le(d + off) / 32768.0f;
    } else { /* 8 bit unsigned */
        size_t off = (size_t)frame * ch + (size_t)c;
        if (off >= sz) return 0.0f;
        return ((float)d[off] - 128.0f) / 128.0f;
    }
}

gk_sound* gk_sound_load(const char* asset_path) {
    size_t size = 0;
    unsigned char* raw;
    size_t pos;
    int fmt = -1, ch = 0, rate = 0, bits = 0;
    size_t data_off = 0, data_sz = 0;
    int frames_in, frames_out, i;
    gk_sound* snd;
    double ratio;

    raw = gk_file_read_all(gk_asset_path(asset_path), &size);
    if (!raw) {
        GK_ERROR("sound not found: %s\n", asset_path);
        return NULL;
    }
    if (size < 44 || memcmp(raw, "RIFF", 4) != 0 || memcmp(raw + 8, "WAVE", 4) != 0) {
        GK_ERROR("not a WAV file: %s\n", asset_path);
        free(raw);
        return NULL;
    }

    pos = 12;
    while (pos + 8 <= size) {
        gk_u32 sz = gk__u32le(raw + pos + 4);
        if (memcmp(raw + pos, "fmt ", 4) == 0 && pos + 24 <= size) {
            fmt  = (int)gk__u16le(raw + pos + 8);
            ch   = (int)gk__u16le(raw + pos + 10);
            rate = (int)gk__u32le(raw + pos + 12);
            bits = (int)gk__u16le(raw + pos + 22);
            if (fmt == 0xFFFE && sz >= 40)   /* WAVE_FORMAT_EXTENSIBLE */
                fmt = 1;                      /* assume PCM subformat   */
        } else if (memcmp(raw + pos, "data", 4) == 0) {
            data_off = pos + 8;
            data_sz  = sz;
            if (data_off + data_sz > size)
                data_sz = size - data_off;
            break;
        }
        pos += 8 + (size_t)sz + (sz & 1);
    }

    if (fmt != 1 || (ch != 1 && ch != 2) || (bits != 8 && bits != 16) || rate <= 0 || data_sz == 0) {
        GK_ERROR("unsupported WAV (%s): fmt=%d ch=%d bits=%d rate=%d\n",
                 asset_path, fmt, ch, bits, rate);
        free(raw);
        return NULL;
    }

    frames_in = (int)(data_sz / (size_t)((bits / 8) * ch));
    if (frames_in <= 0) { free(raw); return NULL; }

    snd = (gk_sound*)calloc(1, sizeof(gk_sound));
    if (!snd) { free(raw); return NULL; }

    ratio = (double)rate / (double)GK_MIX_RATE;
    frames_out = (rate == GK_MIX_RATE) ? frames_in
               : (int)((double)frames_in / ratio);
    if (frames_out < 1) frames_out = 1;

    snd->data = (gk_i16*)malloc(sizeof(gk_i16) * 2u * (size_t)frames_out);
    if (!snd->data) { free(snd); free(raw); return NULL; }
    snd->frames = frames_out;

    {
        const unsigned char* d = raw + data_off;
        for (i = 0; i < frames_out; i++) {
            double sp = (rate == GK_MIX_RATE)
                      ? (double)i
                      : (double)i * ratio;
            int s0 = (int)sp;
            int s1 = s0 + 1 < frames_in ? s0 + 1 : frames_in - 1;
            float frac = (float)(sp - (double)s0);
            float l, r;
            if (s0 >= frames_in) s0 = frames_in - 1;
            l = gk__wav_sample(d, data_sz, bits, ch, s0, 0) * (1.0f - frac)
              + gk__wav_sample(d, data_sz, bits, ch, s1, 0) * frac;
            if (ch == 2) {
                r = gk__wav_sample(d, data_sz, bits, ch, s0, 1) * (1.0f - frac)
                  + gk__wav_sample(d, data_sz, bits, ch, s1, 1) * frac;
            } else {
                r = l;
            }
            snd->data[2 * i]     = (short)(gk_clampf(l, -1.0f, 1.0f) * 32767.0f);
            snd->data[2 * i + 1] = (short)(gk_clampf(r, -1.0f, 1.0f) * 32767.0f);
        }
    }
    snd->dur = (float)((double)frames_out / (double)GK_MIX_RATE);
    GK_LOG("sound %s: %.2fs (%d frames @44100 stereo)\n", asset_path, snd->dur, frames_out);
    free(raw);
    return snd;
}

void gk_sound_destroy(gk_sound* snd) {
    if (!snd) return;
    free(snd->data);
    free(snd);
}

float gk_sound_duration(const gk_sound* snd) { return snd ? snd->dur : 0.0f; }

/* ---- mixer thread ----------------------------------------------------------- */

static void gk__mix_chunk(void) {
    static gk_i16 out[2 * GK_MIX_PERIOD];
    float acc[2 * GK_MIX_PERIOD];
    int i, c;

    memset(acc, 0, sizeof acc);

    pthread_mutex_lock(&AU.lock);
    for (c = 0; c < GK_AUDIO_MAX_CHANNELS; c++) {
        gk__voice* v = &AU.ch[c];
        if (!v->active || !v->snd) continue;
        {
            const gk_i16* d = v->snd->data;
            int frames = v->snd->frames;
            float lg = v->vol * (v->pan <= 0.0f ? 1.0f : 1.0f - v->pan);
            float rg = v->vol * (v->pan >= 0.0f ? 1.0f : 1.0f + v->pan);
            for (i = 0; i < GK_MIX_PERIOD; i++) {
                if (v->pos >= frames) {
                    if (v->loop) {
                        v->pos = 0;
                    } else {
                        v->active = false;
                        break;
                    }
                }
                acc[2 * i]     += (float)d[2 * v->pos]     / 32768.0f * lg;
                acc[2 * i + 1] += (float)d[2 * v->pos + 1] / 32768.0f * rg;
                v->pos++;
            }
        }
    }
    pthread_mutex_unlock(&AU.lock);

    for (i = 0; i < GK_MIX_PERIOD; i++) {
        float l = gk_clampf(acc[2 * i]     * AU.master, -1.0f, 1.0f);
        float r = gk_clampf(acc[2 * i + 1] * AU.master, -1.0f, 1.0f);
        out[2 * i]     = (short)(l * 32767.0f);
        out[2 * i + 1] = (short)(r * 32767.0f);
    }

    if (AU.ok && AU.pcm) {
        snd_pcm_sframes_t written = snd_pcm_writei(AU.pcm, out, GK_MIX_PERIOD);
        if (written == -EPIPE) {
            snd_pcm_prepare(AU.pcm);
        } else if (written >= 0 && written < GK_MIX_PERIOD) {
            snd_pcm_writei(AU.pcm, out + 2 * written, GK_MIX_PERIOD - written);
        } else if (written < 0) {
            snd_pcm_prepare(AU.pcm);
        }
    }
}

static void* gk__mix_thread(void* arg) {
    (void)arg;
    while (AU.running) {
        if (AU.ok) {
            gk__mix_chunk();
        } else {
            usleep(10000);
        }
    }
    return NULL;
}

/* ---- public API --------------------------------------------------------------- */

int gk_audio_init(void) {
    snd_pcm_hw_params_t* hw;
    unsigned rate = GK_MIX_RATE;
    int dir = 0;
    snd_pcm_uframes_t period = GK_MIX_PERIOD;
    snd_pcm_uframes_t buffer = GK_MIX_PERIOD * 4;

    memset(&AU, 0, sizeof AU);
    AU.master = 1.0f;
    pthread_mutex_init(&AU.lock, NULL);

    if (snd_pcm_open(&AU.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        GK_WARN("ALSA: cannot open 'default' device — audio disabled\n");
        AU.ok = false;
        return 0;
    }
    snd_pcm_hw_params_alloca(&hw);
    if (snd_pcm_hw_params_any(AU.pcm, hw) < 0 ||
        snd_pcm_hw_params_set_rate_resample(AU.pcm, hw, 1) < 0 ||
        snd_pcm_hw_params_set_access(AU.pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
        snd_pcm_hw_params_set_format(AU.pcm, hw, SND_PCM_FORMAT_S16_LE) < 0 ||
        snd_pcm_hw_params_set_channels(AU.pcm, hw, 2) < 0 ||
        snd_pcm_hw_params_set_rate_near(AU.pcm, hw, &rate, &dir) < 0) {
        GK_WARN("ALSA: hw params failed — audio disabled\n");
        snd_pcm_close(AU.pcm);
        AU.pcm = NULL;
        AU.ok = false;
        return 0;
    }
    snd_pcm_hw_params_set_period_size_near(AU.pcm, hw, &period, &dir);
    snd_pcm_hw_params_set_buffer_size_near(AU.pcm, hw, &buffer);
    if (snd_pcm_hw_params(AU.pcm, hw) < 0 || snd_pcm_prepare(AU.pcm) < 0) {
        GK_WARN("ALSA: prepare failed — audio disabled\n");
        snd_pcm_close(AU.pcm);
        AU.pcm = NULL;
        AU.ok = false;
        return 0;
    }

    AU.ok = true;
    AU.running = true;
    if (pthread_create(&AU.thread, NULL, gk__mix_thread, NULL) != 0) {
        GK_WARN("ALSA: mixer thread failed — audio disabled\n");
        AU.running = false;
        AU.ok = false;
        snd_pcm_close(AU.pcm);
        AU.pcm = NULL;
        return 0;
    }
    GK_LOG("audio: ALSA %u Hz stereo, %lu-frame periods\n",
           rate, (unsigned long)period);
    return 0;
}

bool gk_audio_ready(void) { return AU.ok; }

int gk_sound_play(const gk_sound* snd, float vol, float pan, bool loop) {
    int i, pick = -1;
    if (!AU.ok || !snd) return -1;
    pthread_mutex_lock(&AU.lock);
    for (i = 0; i < GK_AUDIO_MAX_CHANNELS; i++) {
        if (!AU.ch[i].active) { pick = i; break; }
    }
    if (pick < 0) {
        /* steal the oldest voice */
        gk_u32 best = 0xFFFFFFFFu;
        for (i = 0; i < GK_AUDIO_MAX_CHANNELS; i++) {
            if (AU.ch[i].stamp < best) { best = AU.ch[i].stamp; pick = i; }
        }
    }
    if (pick >= 0) {
        gk__voice* v = &AU.ch[pick];
        v->snd   = snd;
        v->pos   = 0;
        v->vol   = gk_maxf(0.0f, vol);
        v->pan   = gk_clampf(pan, -1.0f, 1.0f);
        v->loop  = loop;
        v->active = true;
        v->stamp = ++AU.play_counter;
    }
    pthread_mutex_unlock(&AU.lock);
    return pick;
}

void gk_channel_stop(int ch) {
    if (ch < 0 || ch >= GK_AUDIO_MAX_CHANNELS) return;
    pthread_mutex_lock(&AU.lock);
    AU.ch[ch].active = false;
    pthread_mutex_unlock(&AU.lock);
}

void gk_channel_set_volume(int ch, float vol) {
    if (ch < 0 || ch >= GK_AUDIO_MAX_CHANNELS) return;
    pthread_mutex_lock(&AU.lock);
    AU.ch[ch].vol = gk_maxf(0.0f, vol);
    pthread_mutex_unlock(&AU.lock);
}

void gk_channel_set_pan(int ch, float pan) {
    if (ch < 0 || ch >= GK_AUDIO_MAX_CHANNELS) return;
    pthread_mutex_lock(&AU.lock);
    AU.ch[ch].pan = gk_clampf(pan, -1.0f, 1.0f);
    pthread_mutex_unlock(&AU.lock);
}

void gk_audio_set_master(float vol) {
    AU.master = gk_maxf(0.0f, vol);
}

void gk_audio_stop_all(void) {
    int i;
    pthread_mutex_lock(&AU.lock);
    for (i = 0; i < GK_AUDIO_MAX_CHANNELS; i++)
        AU.ch[i].active = false;
    pthread_mutex_unlock(&AU.lock);
}

void gk_audio_shutdown(void) {
    if (AU.running) {
        AU.running = false;
        pthread_join(AU.thread, NULL);
    }
    gk_audio_stop_all();
    if (AU.pcm) {
        snd_pcm_drain(AU.pcm);
        snd_pcm_close(AU.pcm);
        AU.pcm = NULL;
    }
    AU.ok = false;
    pthread_mutex_destroy(&AU.lock);
}
