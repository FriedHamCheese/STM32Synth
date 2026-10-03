#include "waveform.h"
#include <math.h>
#include <stddef.h>

#define PI 3.14159265358979323846f

/* One full period per shape, sampled at WAVETABLE_SIZE+1 points (values -1..+1).
   The +1 point makes linear interpolation across the wrap cheap and correct. */
#define WAVETABLE_SIZE 512

static float wavetable[WAVE_SHAPE_COUNT][WAVETABLE_SIZE + 1];
static int wavetable_initialized = 0;

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value > 1.0f)
        return 1.0f;

    return value;
}

/* Value of `shape` at `position`, where position runs 0..1 across one period. */
static float shape_value_at(WaveShape shape, float position)
{
    switch (shape)
    {
        case WAVE_SINE:
            return sinf(position * 2.0f * PI);

        case WAVE_TRIANGLE:
            /* -1 at 0, +1 at 0.5, -1 at 1 */
            return 1.0f - 4.0f * fabsf(position - 0.5f);

        case WAVE_SQUARE:
            return position < 0.5f ? 1.0f : -1.0f;

        case WAVE_SAWTOOTH:
            /* -1 at 0 rising to +1 at 1, then wraps back to -1 */
            return 2.0f * position - 1.0f;

        default:
            return sinf(position * 2.0f * PI);
    }
}

void waveform_init(void)
{
    if (wavetable_initialized)
        return;

    for (int shape = 0; shape < WAVE_SHAPE_COUNT; shape++)
        for (int i = 0; i <= WAVETABLE_SIZE; i++)
            wavetable[shape][i] = shape_value_at((WaveShape)shape,
                                                 (float)i / (float)WAVETABLE_SIZE);

    wavetable_initialized = 1;
}

static float wavetable_lookup(WaveShape shape, float phase)
{
    int s = (int)shape;
    float position;
    int index;
    float fraction;

    if (s < 0 || s >= WAVE_SHAPE_COUNT)
        s = WAVE_SINE;

    phase -= floorf(phase);                 /* wrap into 0..1 */

    position = phase * (float)WAVETABLE_SIZE;
    index = (int)position;

    if (index >= WAVETABLE_SIZE)
        index = WAVETABLE_SIZE - 1;

    fraction = position - (float)index;

    return wavetable[s][index] +
           fraction * (wavetable[s][index + 1] - wavetable[s][index]);
}

wavegen_output_t waveform_get_wavetable_point(WaveShape shape, float phase, float max_output)
{
    float value;

    if (!isfinite(phase) || !isfinite(max_output) || max_output <= 0.0f)
        return 0;

    waveform_init();

    /* bipolar -1..+1 mapped to 0..1, where 0.5 is the centre */
    value = clamp01((wavetable_lookup(shape, phase) + 1.0f) * 0.5f);

    return (wavegen_output_t)(value * max_output);
}

wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config)
{
    if (config == NULL)
        return 0;

    return waveform_get_wavetable_point(config->shape, phase, config->max_output);
}

wavegen_output_t get_sine_point(float phase, float max_output)
{
    return waveform_get_wavetable_point(WAVE_SINE, phase, max_output);
}

wavegen_output_t get_triangle_point(float phase, float max_output)
{
    return waveform_get_wavetable_point(WAVE_TRIANGLE, phase, max_output);
}

wavegen_output_t get_square_point(float phase, float max_output)
{
    return waveform_get_wavetable_point(WAVE_SQUARE, phase, max_output);
}

wavegen_output_t get_sawtooth_point(float phase, float max_output)
{
    return waveform_get_wavetable_point(WAVE_SAWTOOTH, phase, max_output);
}
