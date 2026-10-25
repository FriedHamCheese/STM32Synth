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

void waveform_prepare(const WaveformConfig *config, WaveformPrepared *prepared)
{
    if (prepared == 0)
        return;

    prepared->valid = 0;

    if (config == NULL || config->max_output <= 0.0f)
        return;

    float rise = config->rise_pct;
    float fall = config->fall_pct;

    if (rise < 0.001f)
        rise = 0.001f;

    if (fall < 0.001f)
        fall = 0.001f;

    const float total = rise + fall;
    if (total > 1.0f)
    {
        rise /= total;
        fall /= total;
    }

    float midpoint = config->midpoint;
    if (midpoint <= 0.0f)
        midpoint = 0.001f;
    if (midpoint >= 1.0f)
        midpoint = 0.999f;

    prepared->midpoint = midpoint;
    prepared->rise = rise;
    prepared->fall = fall;
    prepared->fall_start = 1.0f - fall;
    prepared->inv_midpoint = 1.0f / midpoint;
    prepared->inv_trough_span = 1.0f / (1.0f - midpoint);
    prepared->inv_rise = 1.0f / rise;
    prepared->inv_fall = 1.0f / fall;
    prepared->valid = 1;
}

wavegen_output_t waveform_get_point_prepared(
    float phase,
    const WaveformConfig *config,
    const WaveformPrepared *prepared
) {
    if (prepared == 0 || !prepared->valid)
        return 0.0f;

    float value;

    if (phase < prepared->midpoint)
    {
        const float crest_ratio = phase * prepared->inv_midpoint;

        if (crest_ratio < prepared->rise)
        {
            const float x = crest_ratio * prepared->inv_rise;
            value = get_shape_value(config->rise_shape, x, 1);
        }
        else if (crest_ratio < prepared->fall_start)
        {
            value = 1.0f;
        }
        else
        {
            const float x = (crest_ratio - prepared->fall_start)
                * prepared->inv_fall;
            value = get_shape_value(config->fall_shape, x, 0);
        }
    }
    else
    {
        const float trough_ratio = (phase - prepared->midpoint)
            * prepared->inv_trough_span;

        if (trough_ratio < prepared->rise)
        {
            const float x = trough_ratio * prepared->inv_rise;
            value = -get_shape_value(config->rise_shape, x, 1);
        }
        else if (trough_ratio < prepared->fall_start)
        {
            value = -1.0f;
        }
        else
        {
            const float x = (trough_ratio - prepared->fall_start)
                * prepared->inv_fall;
            value = -get_shape_value(config->fall_shape, x, 0);
        }
    }

    value = (value + 1.0f) * 0.5f;

    return (wavegen_output_t)(value * config->max_output);
}

wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config)
{
    WaveformPrepared prepared;
    waveform_prepare(config, &prepared);
    return waveform_get_point_prepared(phase, config, &prepared);
}
