#include "adsr.h"
#include "sine_lookup.h"

//Used when no AdsrParam is given: instant attack, full sustain, instant release, same as having no envelope
static const AdsrParam no_envelope = {
    .attack_step = 1.0f,
    .decay_step = 1.0f,
    .sustain_level = 1.0f,
    .release_step = 1.0f
};

//Volume change per sample for a full 0<->1 swing over time_ms, capped at 1.0 (instant)
static float time_to_step(uint32_t time_ms, float sample_period_ms)
{
    if ((float)time_ms <= sample_period_ms)
        return 1.0f;
    return sample_period_ms / (float)time_ms;
}

void adsr_set_params(
    AdsrParam *param,
    uint32_t attack_ms,
    uint32_t decay_ms,
    float sustain_level,
    uint32_t release_ms,
    float sample_period
){
    const float sample_period_ms = sample_period * 1000.0f;

    param->attack_step = time_to_step(attack_ms, sample_period_ms);
    param->decay_step = time_to_step(decay_ms, sample_period_ms);
    param->sustain_level = clamp01(sustain_level);
    param->release_step = time_to_step(release_ms, sample_period_ms);
}

void adsr_init(AdsrState *state)
{
    state->stage = ADSR_IDLE;
    state->volume = 0.0f;
}

void adsr_note_on(AdsrState *state)
{
    state->stage = ADSR_ATTACK;
}

void adsr_note_off(AdsrState *state)
{
    if (state->stage == ADSR_IDLE)
        return;
    state->stage = ADSR_RELEASE;
}

//Steps volume toward target, landing exactly on target instead of overshooting it
static float move_toward(float volume, float target, float step)
{
    if (volume < target)
        return (volume + step < target) ? volume + step : target;
    return (volume - step > target) ? volume - step : target;
}

float adsr_get_volume(AdsrState *state, const AdsrParam *param)
{
    if (param == 0)
        param = &no_envelope;

    switch (state->stage)
    {
        case ADSR_ATTACK:
            state->volume += param->attack_step;
            if (state->volume >= 1.0f)
            {
                state->volume = 1.0f;
                state->stage = ADSR_DECAY;
            }
            break;

        case ADSR_DECAY:
            state->volume = move_toward(state->volume, param->sustain_level, param->decay_step);
            if (state->volume == param->sustain_level)
                state->stage = ADSR_SUSTAIN;
            break;

        case ADSR_SUSTAIN:
            //Glides to a changed sustain pot instead of jumping
            state->volume = move_toward(state->volume, param->sustain_level, param->decay_step);
            break;

        case ADSR_RELEASE:
            state->volume -= param->release_step;
            if (state->volume <= 0.0f)
            {
                state->volume = 0.0f;
                state->stage = ADSR_IDLE;
            }
            break;

        case ADSR_IDLE:
        default:
            state->volume = 0.0f;
            break;
    }

    return state->volume;
}

uint8_t adsr_is_active(const AdsrState *state)
{
    return state->stage != ADSR_IDLE;
}

float adsr_apply_volume(wavegen_output_t sample, float volume)
{
    return ((float)sample - WAVEGEN_OUTPUT_GROUND) * volume;
}
