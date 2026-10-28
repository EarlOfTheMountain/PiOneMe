/* =========================================================================
 * gk_audio.h — ALSA software mixer (PCM WAV, 44.1 kHz stereo s16)
 *
 * A background thread mixes up to GK_AUDIO_MAX_CHANNELS voices and writes
 * to the "default" ALSA PCM device. WAV files of any (PCM 8/16-bit,
 * mono/stereo) sample rate are converted on load.
 *
 * If the audio device cannot be opened the framework keeps running with
 * audio silently disabled (gk_audio_ready() == 0).
 * ========================================================================= */
#ifndef GK_AUDIO_H
#define GK_AUDIO_H

#include "gk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GK_AUDIO_MAX_CHANNELS 32

typedef struct gk_sound gk_sound;

/* Loads a PCM WAV from an asset-relative path, e.g. "sounds/blip.wav". */
gk_sound* gk_sound_load(const char* asset_path);
void      gk_sound_destroy(gk_sound* snd);
float     gk_sound_duration(const gk_sound* snd);   /* seconds */

/* vol 0..1+, pan -1..1 (0 = center), loop. Returns a channel handle >= 0,
   or -1 when audio is unavailable or all channels are busy. */
int  gk_sound_play(const gk_sound* snd, float vol, float pan, bool loop);

void gk_channel_stop(int ch);
void gk_channel_set_volume(int ch, float vol);
void gk_channel_set_pan(int ch, float pan);

int  gk_audio_init(void);       /* 0 on success (device may still be silent) */
bool gk_audio_ready(void);      /* true when the ALSA device was opened */
void gk_audio_set_master(float vol);
void gk_audio_stop_all(void);
void gk_audio_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* GK_AUDIO_H */
