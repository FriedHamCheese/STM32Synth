#include "waveform.h"
#include "sine_lookup.h"
#include <stddef.h>

__attribute__((always_inline))
static inline float sin_quarter_circle(float wave_completion){
  return sine_lookup(wave_completion * 0.25f);
}

static float get_shape_value(WaveShape shape, float x, int rising)
{
    /* x is in [0,1]; controls clamp pot values before they reach here. */
    switch (shape)
    {
        case WAVE_SINE:
            return rising
                ? sin_quarter_circle(x)
                : sin_quarter_circle(1.0f - x);

        case WAVE_TRIANGLE:
            return rising ? x : 1.0f - x;

        case WAVE_SQUARE:
            return rising ? 1.0f : 0.0f;

        default:
            return rising ? x : 1.0f - x;
    }
}

/*
 * Fast path. Preconditions guaranteed by callers:
 *  - phase is finite and in [0,1)
 *  - config is non-NULL; max_output > 0; midpoint is in (0,1)
 * controls_update() clamps pot-derived values; voice_manager_get_sample()
 * wraps phase before calling this function.
 */
wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config)
{
    float rise;
    float fall;
    float hold;
    float total;
    float value;

    if (config == NULL || config->max_output <= 0.0f)
        return 0.0f;

    rise = config->rise_pct;
    fall = config->fall_pct;

    if (rise < 0.001f)
        rise = 0.001f;

    if (fall < 0.001f)
        fall = 0.001f;

    total = rise + fall;

    if (total > 1.0f)
    {
        rise /= total;
        fall /= total;
    }
    
    float local_midpoint = config->midpoint;
    if(local_midpoint <= 0.0f){
      local_midpoint = 0.001f;
    }

    hold = 1.0f - rise - fall;
    if (phase < local_midpoint)
    {
        const float crest_ratio = phase/local_midpoint;

        if (crest_ratio < rise)
        {
            float x = crest_ratio / rise;
            value = get_shape_value(config->rise_shape, x, 1);
        }
        else if (crest_ratio < rise + hold)
        {
            value = 1.0f;
        }
        else
        {
            float x = (crest_ratio - rise - hold) / fall;
            value = get_shape_value(config->fall_shape, x, 0);
        }
    }
    else
    {
        const float trough_ratio = (phase-local_midpoint)/(1.0f-local_midpoint);

        if (trough_ratio < rise)
        {
            float x = trough_ratio / rise;
            value = -get_shape_value(config->rise_shape, x, 1);
        }
        else if (trough_ratio < rise + hold)
        {
            value = -1.0f;
        }
        else
        {
            float x = (trough_ratio - rise - hold) / fall;
            value = -get_shape_value(config->fall_shape, x, 0);
        }
    }
    value = (value + 1.0f) * 0.5f;

    return (wavegen_output_t)(value * config->max_output);
}
