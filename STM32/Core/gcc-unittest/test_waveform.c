#include "waveform.h"
#include "sine_lookup.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define TEST_MAX_OUTPUT 349.0f
#define TEST_OUTPUT_TOLERANCE 1.0f

/* Reference: waveform_get_point at 48319c4^ (before the optimization commit). */
static float ref_sin_quarter_circle(float wave_completion)
{
    return sine_lookup(wave_completion * 0.25f);
}

static float ref_get_shape_value(WaveShape shape, float x, int rising)
{
    x = clamp01(x);

    switch (shape)
    {
        case WAVE_SINE:
            return rising
                ? ref_sin_quarter_circle(x)
                : ref_sin_quarter_circle(1.0f - x);

        case WAVE_TRIANGLE:
            return rising ? x : 1.0f - x;

        case WAVE_SQUARE:
            return rising ? 1.0f : 0.0f;

        default:
            return rising ? x : 1.0f - x;
    }
}

static wavegen_output_t ref_waveform_get_point(float phase, const WaveformConfig *config)
{
    float rise;
    float fall;
    float hold;
    float total;
    float value;

    if (config == NULL ||
        !isfinite(phase) ||
        !isfinite(config->max_output) ||
        config->max_output <= 0.0f)
        return 0.0f;

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

    float local_midpoint = config->midpoint;
    if (local_midpoint <= 0.0f)
        local_midpoint = 0.001f;

    hold = 1.0f - rise - fall;

    if (phase < local_midpoint)
    {
        const float crest_ratio = phase/local_midpoint;

        if (crest_ratio < rise)
        {
            float x = crest_ratio / rise;
            value = ref_get_shape_value(config->rise_shape, x, 1);
        }
        else if (crest_ratio < rise + hold)
        {
            value = 1.0f;
        }
        else
        {
            float x = (crest_ratio - rise - hold) / fall;
            value = ref_get_shape_value(config->fall_shape, x, 0);
        }
    }
    else
    {
        const float trough_ratio = (phase-local_midpoint)/(1.0-local_midpoint);

        if (trough_ratio < rise)
        {
            float x = trough_ratio / rise;
            value = -ref_get_shape_value(config->rise_shape, x, 1);
        }
        else if (trough_ratio < rise + hold)
        {
            value = -1.0f;
        }
        else
        {
            float x = (trough_ratio - rise - hold) / fall;
            value = -ref_get_shape_value(config->fall_shape, x, 0);
        }
    }

    value = (value + 1.0f) * 0.5f;

    return (wavegen_output_t)(value * config->max_output);
}

static void test_matches_reference(void)
{
    printf("\ttest_waveform_matches_reference()...\n");

    const WaveShape shapes[] = { WAVE_SINE, WAVE_TRIANGLE, WAVE_SQUARE };
    const float levels[] = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f };
    const float midpoints[] = { 0.001f, 0.25f, 0.5f, 0.75f, 0.999f };
    const int phase_steps = 2048;

    for (size_t rs = 0; rs < sizeof(shapes)/sizeof(shapes[0]); rs++) {
        for (size_t fs = 0; fs < sizeof(shapes)/sizeof(shapes[0]); fs++) {
            for (size_t r = 0; r < sizeof(levels)/sizeof(levels[0]); r++) {
                for (size_t f = 0; f < sizeof(levels)/sizeof(levels[0]); f++) {
                    for (size_t m = 0; m < sizeof(midpoints)/sizeof(midpoints[0]); m++) {
                        const WaveformConfig cfg = {
                            .rise_pct = levels[r],
                            .fall_pct = levels[f],
                            .rise_shape = shapes[rs],
                            .fall_shape = shapes[fs],
                            .max_output = TEST_MAX_OUTPUT,
                            .midpoint = midpoints[m]
                        };

                        for (int i = 0; i < phase_steps; i++) {
                            const float phase = (float)i / (float)phase_steps;
                            const wavegen_output_t expected =
                                ref_waveform_get_point(phase, &cfg);
                            const wavegen_output_t actual =
                                waveform_get_point(phase, &cfg);
                            const float diff = fabsf(
                                (float)actual - (float)expected);

                            if (diff > TEST_OUTPUT_TOLERANCE) {
                                printf("  mismatch phase=%.6f rise=%.2f fall=%.2f "
                                       "mid=%.3f shapes=%d/%d expected=%u actual=%u\n",
                                       phase, cfg.rise_pct, cfg.fall_pct, cfg.midpoint,
                                       (int)cfg.rise_shape, (int)cfg.fall_shape,
                                       (unsigned)expected, (unsigned)actual);
                            }
                            assert(diff <= TEST_OUTPUT_TOLERANCE);
                        }
                    }
                }
            }
        }
    }
}

static void test_output_bounds(void)
{
    printf("\ttest_waveform_output_bounds()...\n");

    const WaveShape shapes[] = { WAVE_SINE, WAVE_TRIANGLE, WAVE_SQUARE };
    const float midpoints[] = { 0.001f, 0.25f, 0.5f, 0.75f, 0.999f };

    for (size_t rs = 0; rs < sizeof(shapes)/sizeof(shapes[0]); rs++) {
        for (size_t fs = 0; fs < sizeof(shapes)/sizeof(shapes[0]); fs++) {
            for (size_t m = 0; m < sizeof(midpoints)/sizeof(midpoints[0]); m++) {
                const WaveformConfig cfg = {
                    .rise_pct = 0.5f,
                    .fall_pct = 0.5f,
                    .rise_shape = shapes[rs],
                    .fall_shape = shapes[fs],
                    .max_output = TEST_MAX_OUTPUT,
                    .midpoint = midpoints[m]
                };

                for (int i = 0; i < 2048; i++) {
                    const float phase = (float)i / 2048.0f;
                    const wavegen_output_t output =
                        waveform_get_point(phase, &cfg);
                    assert(output <= (wavegen_output_t)TEST_MAX_OUTPUT);
                }
            }
        }
    }
}

static void test_zero_output(void)
{
    printf("\ttest_waveform_zero_output()...\n");

    const WaveformConfig cfg = {
        .rise_pct = 0.5f,
        .fall_pct = 0.5f,
        .rise_shape = WAVE_SINE,
        .fall_shape = WAVE_SINE,
        .max_output = 0.0f,
        .midpoint = 0.5f
    };

    assert(waveform_get_point(0.25f, &cfg) == 0);
}

void test_waveform(void)
{
    printf("test_waveform()\n");
    test_matches_reference();
    test_output_bounds();
    test_zero_output();
}
