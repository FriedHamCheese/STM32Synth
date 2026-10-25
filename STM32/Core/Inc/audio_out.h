#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H

#include "waveform.h"
#include "voice_manager.h"
#include "volume_oscillation.h"
#include "metronome.h"

#define AUDIO_SAMPLE_RATE_HZ 12000.0f
//Avoid 14-cycle float division instructions later on, use 1-3 cycle multiplication
#define AUDIO_SAMPLE_PERIOD 1.0f/AUDIO_SAMPLE_RATE_HZ

/* TIM2 PWM period; keep the mixed sample inside the compare range. */
#define AUDIO_OUTPUT_MIN 0.0f
#define AUDIO_OUTPUT_MAX 350.0f

/*
 * Voice and click are centered amplitudes. The click follows master volume
 * but not the LFO, so a tremolo setting cannot fade the metronome out.
 * Host-testable: no HAL dependency.
 */
static inline float audio_mix_sample(
    float voice_sample,
    float click,
    float master_volume,
    float lfo_multiplier
) {
    float mixed = (
        (voice_sample - WAVEGEN_OUTPUT_GROUND)
        * lfo_multiplier
        * master_volume
    ) + WAVEGEN_OUTPUT_GROUND + click * master_volume;

    if (mixed < AUDIO_OUTPUT_MIN)
        mixed = AUDIO_OUTPUT_MIN;
    if (mixed > AUDIO_OUTPUT_MAX)
        mixed = AUDIO_OUTPUT_MAX;

    return mixed;
}

/*
 * One-pole DC blocker on the centered sample. Midpoint/rise/fall changes
 * shift the waveform mean by up to ~112 PWM counts; summing voices would
 * otherwise put that offset on the speaker. Cutoff ~9.5 Hz at 12 kHz.
 */
#define AUDIO_DC_BLOCK_R 0.995f

typedef struct {
    float previous_input;
    float previous_output;
} AudioDcBlockState;

static inline float audio_dc_block(AudioDcBlockState *state, float centered)
{
    const float output = centered
        - state->previous_input
        + AUDIO_DC_BLOCK_R * state->previous_output;
    state->previous_input = centered;
    state->previous_output = output;
    return output;
}

void audio_out_init(VoiceManager *vm, WaveformConfig *cfg, VolumeOscillationParam *voscp, float *master_volume, MetronomeParam *metro);
void audio_out_tick(void);
void set_audio_output_value(uint16_t left, uint16_t right);

/* HAL_TIM_PeriodElapsedCallback defined in audio_out.c */

#endif /* __AUDIO_OUT_H */
