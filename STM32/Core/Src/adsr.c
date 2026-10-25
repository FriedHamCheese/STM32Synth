#include "adsr.h"
#include "sine_lookup.h"

//Used when no AdsrParam is given: instant attack, full sustain, instant release, same as having no envelope
static const AdsrParam no_envelope = {
    .attack_rate = 1.0f,
    .decay_rate = 1.0f,
    .sustain_level = 1.0f,
    .release_rate = 1.0f
};

//Fraction of a time_ms long stage completed per sample, capped at 1.0 (instant)
static float time_to_rate(uint32_t time_ms, float sample_period_ms)
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
    float sample_rate_hz
){
    const float sample_period_ms = 1000.0f / sample_rate_hz;

    param->attack_rate = time_to_rate(attack_ms, sample_period_ms);
    param->decay_rate = time_to_rate(decay_ms, sample_period_ms);
    param->sustain_level = clamp01(sustain_level);
    param->release_rate = time_to_rate(release_ms, sample_period_ms);
}

void adsr_init(AdsrState *state)
{
    state->stage = ADSR_IDLE;
    state->volume = 0.0f;
    state->release_start_volume = 0.0f;
}

void adsr_note_on(AdsrState *state)
{
    state->stage = ADSR_ATTACK;
}

void adsr_note_off(AdsrState *state)
{
    if (state->stage == ADSR_IDLE || state->stage == ADSR_RELEASE)
        return;
    state->stage = ADSR_RELEASE;
    state->release_start_volume = state->volume;
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
            //0 -> 1 is the whole attack, so the rate is also the volume step
            state->volume += param->attack_rate;
            if (state->volume >= 1.0f)
            {
                state->volume = 1.0f;
                state->stage = ADSR_DECAY;
            }
            break;

        case ADSR_DECAY:
            //Sustain pot turned above the current volume, nothing left to decay
            if (state->volume <= param->sustain_level)
            {
                state->stage = ADSR_SUSTAIN;
                break;
            }
            //Step scales with the distance 1 -> sustain so decay takes decay_ms at any sustain level
            state->volume = move_toward(
                state->volume,
                param->sustain_level,
                (1.0f - param->sustain_level) * param->decay_rate
            );
            if (state->volume == param->sustain_level)
                state->stage = ADSR_SUSTAIN;
            break;

        case ADSR_SUSTAIN:
            //Glides to a changed sustain pot instead of jumping
            state->volume = move_toward(
                state->volume,
                param->sustain_level,
                param->decay_rate
            );
            break;

        case ADSR_RELEASE:
            //Step scales with the release start volume so release takes release_ms from any volume
            state->volume -= state->release_start_volume * param->release_rate;
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
