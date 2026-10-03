#include "test_waveform.h"
#include "waveform.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_MAX_OUTPUT 350.0f
#define TEST_MIDPOINT   (TEST_MAX_OUTPUT * 0.5f)
#define TEST_TOLERANCE  3
#define TEST_PI         3.14159265358979323846f

static WaveformConfig make_config(WaveShape shape)
{
    WaveformConfig config;
    config.shape      = shape;
    config.max_output = TEST_MAX_OUTPUT;
    config.rise_pct   = 0.5f;
    config.fall_pct   = 0.5f;
    config.rise_shape = shape;
    config.fall_shape = shape;
    return config;
}

static int point(WaveShape shape, float phase)
{
    return waveform_get_wavetable_point(shape, phase, TEST_MAX_OUTPUT);
}

static void test_sine_matches_math(void)
{
    printf("\t\ttest_sine_matches_math()...\n");

    waveform_init();

    for (int i = 0; i < 200; i++)
    {
        float phase = (float)i / 200.0f;
        float expected = (sinf(2.0f * TEST_PI * phase) + 1.0f) * 0.5f * TEST_MAX_OUTPUT;
        int value = point(WAVE_SINE, phase);
        assert(abs(value - (int)expected) <= TEST_TOLERANCE);
    }
}

static void test_every_shape_reaches_both_extremes(void)
{
    printf("\t\ttest_every_shape_reaches_both_extremes()...\n");

    waveform_init();

    for (int s = 0; s < WAVE_SHAPE_COUNT; s++)
    {
        int lo = 1000000, hi = -1000000;
        for (int i = 0; i < 1000; i++)
        {
            int value = point((WaveShape)s, (float)i / 1000.0f);
            if (value < lo) lo = value;
            if (value > hi) hi = value;
        }
        assert(lo <= 2);
        assert(hi >= (int)TEST_MAX_OUTPUT - 2);
    }
}

static void test_square_is_two_levels(void)
{
    printf("\t\ttest_square_is_two_levels()...\n");

    waveform_init();

    /* Away from the 0.5 edge the square sits at one of the two extremes; the
       edge itself is a single swept sample because of linear interpolation. */
    for (int i = 0; i < 1000; i++)
    {
        float phase = (float)i / 1000.0f;
        int value = point(WAVE_SQUARE, phase);

        if (phase > 0.02f && phase < 0.48f)
            assert(value >= (int)TEST_MAX_OUTPUT - 2);
        else if (phase > 0.52f && phase < 0.98f)
            assert(value <= 2);
    }
}

static void test_triangle_peaks_at_half_cycle(void)
{
    printf("\t\ttest_triangle_peaks_at_half_cycle()...\n");

    waveform_init();

    assert(point(WAVE_TRIANGLE, 0.0f)   <= 2);
    assert(point(WAVE_TRIANGLE, 0.5f)   >= (int)TEST_MAX_OUTPUT - 2);
    assert(point(WAVE_TRIANGLE, 0.999f) <= 2);
}

static void test_sawtooth_ramps_up(void)
{
    printf("\t\ttest_sawtooth_ramps_up()...\n");

    waveform_init();

    int previous = point(WAVE_SAWTOOTH, 0.0f);
    for (int i = 1; i < 1000; i++)
    {
        int value = point(WAVE_SAWTOOTH, (float)i / 1000.0f);
        assert(value >= previous - TEST_TOLERANCE);
        previous = value;
    }
    assert(previous >= (int)TEST_MAX_OUTPUT - 4);
}

/* The sine must fall back down after its peak - the original "won't go down" bug. */
static void test_sine_descends_after_peak(void)
{
    printf("\t\ttest_sine_descends_after_peak()...\n");

    waveform_init();

    assert(point(WAVE_SINE, 0.50f) < point(WAVE_SINE, 0.25f));
    assert(point(WAVE_SINE, 0.75f) < TEST_MIDPOINT);
    assert(point(WAVE_SINE, 0.75f) <= 2);
}

static void test_get_point_uses_config_shape(void)
{
    printf("\t\ttest_get_point_uses_config_shape()...\n");

    waveform_init();

    for (int s = 0; s < WAVE_SHAPE_COUNT; s++)
    {
        WaveformConfig config = make_config((WaveShape)s);
        for (int i = 0; i < 50; i++)
        {
            float phase = (float)i / 50.0f;
            assert(waveform_get_point(phase, &config) == point((WaveShape)s, phase));
        }
    }
}

static void test_phase_wraps_at_one(void)
{
    printf("\t\ttest_phase_wraps_at_one()...\n");

    waveform_init();

    assert(point(WAVE_SINE, 1.0f) == point(WAVE_SINE, 0.0f));
    assert(point(WAVE_SINE, 2.5f) == point(WAVE_SINE, 0.5f));
}

void test_waveform(void)
{
    printf("\ttest_waveform()...\n");
    test_sine_matches_math();
    test_every_shape_reaches_both_extremes();
    test_square_is_two_levels();
    test_triangle_peaks_at_half_cycle();
    test_sawtooth_ramps_up();
    test_sine_descends_after_peak();
    test_get_point_uses_config_shape();
    test_phase_wraps_at_one();
}
