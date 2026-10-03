#include "waveform.h"
#include <math.h>

#define PI 3.14159265358979323846f
#define SINE_TABLE_SIZE 256

static float sine_table[SINE_TABLE_SIZE + 1];
static int sine_table_initialized = 0;

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value > 1.0f)
        return 1.0f;

    return value;
}

void waveform_init(void)
{
    if (sine_table_initialized)
        return;

    for (int i = 0; i <= SINE_TABLE_SIZE; i++)
    {
        float x = (float)i / (float)SINE_TABLE_SIZE;
        sine_table[i] = sinf(x * PI * 0.5f);
    }

    sine_table_initialized = 1;
}

static float sine_lookup(float x)
{
    float position;
    int index;
    float fraction;

    x = clamp01(x);

    position = x * (float)SINE_TABLE_SIZE;
    index = (int)position;

    if (index >= SINE_TABLE_SIZE)
        return sine_table[SINE_TABLE_SIZE];

    fraction = position - (float)index;

    return sine_table[index] +
           fraction * (sine_table[index + 1] - sine_table[index]);
}

static float get_shape_value(WaveShape shape, float x, int rising)
{
    x = clamp01(x);

    switch (shape)
    {
        case WAVE_SINE:
            return rising
                ? sine_lookup(x)
                : sine_lookup(1.0f - x);

        case WAVE_TRIANGLE:
            return rising ? x : 1.0f - x;

        case WAVE_SQUARE:
            return rising ? 1.0f : 0.0f;

        default:
            return rising ? x : 1.0f - x;
    }
}

wavegen_output_t get_sine_point(float phase, float max_output)
{
    float normalized_phase;
    float position;
    float value;
    int quadrant;

    if (!isfinite(phase) || !isfinite(max_output) || max_output <= 0.0f)
        return 0;

    waveform_init();

    normalized_phase = phase - floorf(phase);

    position = normalized_phase * 4.0f;
    quadrant = (int)position;

    if (quadrant > 3)
        quadrant = 3;

    position -= (float)quadrant;

    if (quadrant == 0)
        value = sine_lookup(position);
    else if (quadrant == 1)
        value = sine_lookup(1.0f - position);
    else if (quadrant == 2)
        value = -sine_lookup(position);
    else
        value = -sine_lookup(1.0f - position);

    value = (value + 1.0f) * 0.5f;

    return (wavegen_output_t)(value * max_output);
}

wavegen_output_t get_triangle_point(float phase, float max_output)
{
    float value;

    if (!isfinite(phase) || !isfinite(max_output) || max_output <= 0.0f)
        return 0;

    phase -= floorf(phase);

    if (phase < 0.5f)
        value = phase * 2.0f;
    else
        value = 2.0f - phase * 2.0f;

    return (wavegen_output_t)(value * max_output);
}

wavegen_output_t get_square_point(float phase, float max_output)
{
    if (!isfinite(phase) || !isfinite(max_output) || max_output <= 0.0f)
        return 0;

    phase -= floorf(phase);

    return phase < 0.5f ? (wavegen_output_t)max_output : 0;
}

wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config)
{
    float rise;
    float fall;
    float hold;
    float total;
    float half_phase;
    float value;

    if (config == NULL ||
        !isfinite(phase) ||
        !isfinite(config->max_output) ||
        config->max_output <= 0.0f)
        return 0;

    waveform_init();

    phase -= floorf(phase);

    rise = clamp01(config->rise_pct);
    fall = clamp01(config->fall_pct);

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

    hold = 1.0f - rise - fall;

    if (phase < 0.5f)
    {
        half_phase = phase * 2.0f;

        if (half_phase < rise)
        {
            float x = half_phase / rise;
            value = get_shape_value(config->rise_shape, x, 1);
        }
        else if (half_phase < rise + hold)
        {
            value = 1.0f;
        }
        else
        {
            float x = (half_phase - rise - hold) / fall;
            value = get_shape_value(config->fall_shape, x, 0);
        }
    }
    else
    {
        half_phase = (phase - 0.5f) * 2.0f;

        if (half_phase < rise)
        {
            float x = half_phase / rise;
            value = -get_shape_value(config->rise_shape, x, 1);
        }
        else if (half_phase < rise + hold)
        {
            value = -1.0f;
        }
        else
        {
            float x = (half_phase - rise - hold) / fall;
            value = -get_shape_value(config->fall_shape, x, 0);
        }
    }

    value = (value + 1.0f) * 0.5f;

    return (wavegen_output_t)(value * config->max_output);
}
