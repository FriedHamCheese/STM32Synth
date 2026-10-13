#ifndef ADSR_H
#define ADSR_H

#include <stdint.h>
#include "waveform.h"

/**
  Per-key volume envelope: Attack, Decay, Sustain, Release.

  volume
   1.0 |    /\
       |   /  \________  <- sustain_level
       |  /            \
   0.0 |_/              \____
         A   D    S     R
        ^note_on   ^note_off

  adsr_get_volume() is called once per audio sample by the 48kHz ISR and steps volume by one sample,
  so ramps are smooth at sample resolution and no clock is needed.
  Times are converted to per-sample steps once in adsr_set_params(), so the ISR only adds.

  Each time is how long that stage takes:
  - Attack goes 0.0 -> 1.0 in attack_ms.
  - Decay goes 1.0 -> sustain_level in decay_ms, whatever the sustain level.
  - Release goes from wherever the volume is when the key is released down to 0.0 in release_ms,
    so releasing early (mid attack/decay) fades out over the full release time instead of cutting off.
  - Pressing a key again while it is releasing ramps attack up from the release volume, avoiding a click,
    so that attack is shorter than attack_ms.
  - Turning a knob mid-note only changes the ramp speed from then on, volume never jumps.
  - While sustaining, volume glides to a changed sustain_level, a full 0<->1 glide takes decay_ms.
*/

typedef enum
{
    ADSR_IDLE = 0,
    ADSR_ATTACK,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE
} AdsrStage;

///Shared by all voices. Only change it with adsr_set_params(), safe to call at any time, even mid-note.
///Each rate is the fraction of its stage completed per sample, 1/(stage length in samples).
typedef struct
{
    float attack_rate;
    float decay_rate;
    ///0.0-1.0, volume held after decay while the key is down
    float sustain_level;
    float release_rate;
} AdsrParam;

///One per voice.
typedef struct
{
    AdsrStage stage;
    ///Last value returned by adsr_get_volume()
    float volume;
    ///Volume when the key was released, release scales its step by this so it always takes release_ms
    float release_start_volume;
} AdsrState;

/**
  Sets the envelope from stage times in ms, 0ms means instant.
  Pass AUDIO_SAMPLE_PERIOD as sample_period (seconds per sample), like get_oscillation_completion().
*/
void adsr_set_params(
    AdsrParam *param,
    uint32_t attack_ms,
    uint32_t decay_ms,
    float sustain_level,
    uint32_t release_ms,
    float sample_period
);

void adsr_init(AdsrState *state);
void adsr_note_on(AdsrState *state);
void adsr_note_off(AdsrState *state);

///Advances one sample and returns volume 0.0-1.0. Returns 0.0 and goes ADSR_IDLE once release is done.
///param can be NULL, which acts as no envelope (full volume while held, silent on release).
float adsr_get_volume(AdsrState *state, const AdsrParam *param);
///0 once release has finished, the voice can be freed.
uint8_t adsr_is_active(const AdsrState *state);

///Amplitude of sample (distance from WAVEGEN_OUTPUT_GROUND) scaled by volume.
///Kept as float so voices are mixed before rounding.
float adsr_apply_volume(wavegen_output_t sample, float volume);

#endif
