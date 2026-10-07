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

  Time is in milliseconds, pass HAL_GetTick() as now_ms on the board or a fake clock in unit tests.

  Ramps are rate based: each call moves volume toward the stage target by elapsed_ms/stage_ms.
  Stage times are for a full 0.0<->1.0 swing, a partial swing takes proportionally less,
  e.g. decay_ms = 200 with sustain 0.5 reaches sustain after 100ms.
  - Turning a knob mid-note only changes the ramp speed from then on, volume never jumps.
  - Releasing a key before sustain fades out from wherever attack/decay got to, instead of cutting off.
  - Pressing a key again while it is releasing ramps attack up from the release volume, avoiding a click.
  - While sustaining, volume glides to a changed sustain_level at the decay rate.
*/

typedef enum
{
    ADSR_IDLE = 0,
    ADSR_ATTACK,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE
} AdsrStage;

///Shared by all voices, set by controls. Safe to change at any time, even mid-note.
typedef struct
{
    ///Time for a full 0.0->1.0 rise
    uint32_t attack_ms;
    ///Time for a full 1.0->0.0 fall, used for decay and for gliding to a new sustain_level
    uint32_t decay_ms;
    ///0.0-1.0, volume held after decay while the key is down
    float sustain_level;
    ///Time for a full 1.0->0.0 fall
    uint32_t release_ms;
} AdsrParam;

///One per voice.
typedef struct
{
    AdsrStage stage;
    ///Time volume was last brought up to date
    uint32_t last_ms;
    ///Last value returned by adsr_get_volume()
    float volume;
} AdsrState;

void adsr_init(AdsrState *state);
void adsr_note_on(AdsrState *state, const AdsrParam *param, uint32_t now_ms);
void adsr_note_off(AdsrState *state, const AdsrParam *param, uint32_t now_ms);

///Advances the stage based on now_ms and returns volume 0.0-1.0. Returns 0.0 and goes ADSR_IDLE once release is done.
///param can be NULL in any adsr_* function, which acts as no envelope (full volume while held, silent on release).
float adsr_get_volume(AdsrState *state, const AdsrParam *param, uint32_t now_ms);
///0 once release has finished, the voice can be freed.
uint8_t adsr_is_active(const AdsrState *state);

///Scales the amplitude (distance from WAVEGEN_OUTPUT_GROUND) of sample by volume.
wavegen_output_t adsr_apply_volume(wavegen_output_t sample, float volume);

#endif
