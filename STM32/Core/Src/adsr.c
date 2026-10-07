#include "adsr.h"
#include "sine_lookup.h"

#include <math.h>

//Used when no AdsrParam is given: instant attack, full sustain, instant release, same as having no envelope
static const AdsrParam no_envelope = {
    .attack_ms = 0,
    .decay_ms = 0,
    .sustain_level = 1.0f,
    .release_ms = 0
};

void adsr_init(AdsrState *state)
{
    state->stage = ADSR_IDLE;
    state->last_ms = 0;
    state->volume = 0.0f;
}

void adsr_note_on(AdsrState *state, const AdsrParam *param, uint32_t now_ms)
{
    //Bring volume up to now under the old stage first, so attack starts from the right volume
    adsr_get_volume(state, param, now_ms);
    state->stage = ADSR_ATTACK;
}

void adsr_note_off(AdsrState *state, const AdsrParam *param, uint32_t now_ms)
{
    if (state->stage == ADSR_IDLE || state->stage == ADSR_RELEASE)
        return;
    adsr_get_volume(state, param, now_ms);
    state->stage = ADSR_RELEASE;
}

/**
  Moves volume toward target at a rate of 1.0 per full_scale_ms, spending up to *budget_ms.
  Returns 1 if target was reached, with the unspent time left in *budget_ms for the next stage.
  Returns 0 if the budget ran out first. A 0ms full_scale_ms reaches target instantly.
*/
static uint8_t approach(float *volume, float target, float *budget_ms, uint32_t full_scale_ms)
{
    const float needed_ms = fabsf(target - *volume) * (float)full_scale_ms;

    if (*budget_ms >= needed_ms)
    {
        *volume = target;
        *budget_ms -= needed_ms;
        return 1;
    }

    //Budget is only non-zero once per ms tick, so this division doesn't run every sample
    if (*budget_ms <= 0.0f)
        return 0;

    const float step = *budget_ms / (float)full_scale_ms;
    *volume += (target > *volume) ? step : -step;
    *budget_ms = 0.0f;
    return 0;
}

float adsr_get_volume(AdsrState *state, const AdsrParam *param, uint32_t now_ms)
{
    if (param == 0)
        param = &no_envelope;

    const float sustain_level = clamp01(param->sustain_level);
    float budget_ms = (float)(now_ms - state->last_ms);
    state->last_ms = now_ms;

    //Loop so time left over from a finished stage carries into the next, e.g. 0ms attack goes into decay in the same call
    for (;;)
    {
        switch (state->stage)
        {
            case ADSR_ATTACK:
                if (approach(&state->volume, 1.0f, &budget_ms, param->attack_ms))
                {
                    state->stage = ADSR_DECAY;
                    continue;
                }
                return state->volume;

            case ADSR_DECAY:
                if (approach(&state->volume, sustain_level, &budget_ms, param->decay_ms))
                {
                    state->stage = ADSR_SUSTAIN;
                    continue;
                }
                return state->volume;

            case ADSR_SUSTAIN:
                //Glides to a changed sustain pot instead of jumping
                approach(&state->volume, sustain_level, &budget_ms, param->decay_ms);
                return state->volume;

            case ADSR_RELEASE:
                if (approach(&state->volume, 0.0f, &budget_ms, param->release_ms))
                    state->stage = ADSR_IDLE;
                return state->volume;

            case ADSR_IDLE:
            default:
                state->volume = 0.0f;
                return 0.0f;
        }
    }
}

uint8_t adsr_is_active(const AdsrState *state)
{
    return state->stage != ADSR_IDLE;
}

wavegen_output_t adsr_apply_volume(wavegen_output_t sample, float volume)
{
    const float amplitude = (float)sample - WAVEGEN_OUTPUT_GROUND;
    return (wavegen_output_t)(amplitude * volume + WAVEGEN_OUTPUT_GROUND);
}
