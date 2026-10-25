#include "audio_out.h"
#include "sine_lookup.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint8_t near_float(float actual, float expected)
{
    return fabsf(actual - expected) <= 0.01f;
}

static void test_center_and_clamp(void)
{
    printf("\ttest_audio_mix_center_and_clamp()...\n");

    assert(near_float(audio_mix_sample(175.0f, 0.0f, 1.0f, 1.0f), 175.0f));
    assert(near_float(audio_mix_sample(350.0f, 0.0f, 1.0f, 1.0f), 350.0f));

    /* Master volume > 1 must clamp before the uint16 conversion. */
    assert(audio_mix_sample(175.0f, 97.0f, 10.0f, 1.0f) == 350.0f);
    assert(audio_mix_sample(175.0f, -97.0f, 10.0f, 1.0f) == 0.0f);
}

static void test_click_ignores_lfo(void)
{
    printf("\ttest_audio_mix_click_ignores_lfo()...\n");

    /* LFO at zero mutes voices but must not mute the metronome. */
    assert(near_float(
        audio_mix_sample(350.0f, 50.0f, 1.0f, 0.0f), 225.0f));

    /* Half-depth LFO: voice swings around center, click stays fixed. */
    assert(near_float(
        audio_mix_sample(350.0f, 50.0f, 1.0f, 0.5f), 312.5f));

    /* Master volume 0 mutes everything, including the click. */
    assert(near_float(
        audio_mix_sample(350.0f, 97.0f, 0.0f, 1.0f), 175.0f));
}

static void test_dc_block(void)
{
    printf("\ttest_audio_dc_block()...\n");

    AudioDcBlockState dc_state = { 0.0f, 0.0f };
    float output = 0.0f;
    for (int i = 0; i < 24000; i++)
        output = audio_dc_block(&dc_state, 100.0f);
    assert(fabsf(output) < 1.0f);

    AudioDcBlockState ac_state = { 0.0f, 0.0f };
    float peak = 0.0f;
    for (int i = 0; i < 24000; i++) {
        const float input = 100.0f * sinf(
            2.0f * (float)M_PI * 100.0f * (float)i / 12000.0f
        );
        const float filtered = audio_dc_block(&ac_state, input);
        if (i > 1200 && fabsf(filtered) > peak)
            peak = fabsf(filtered);
    }
    assert(peak > 95.0f);
}

static void test_midpoint_dc_removed(void)
{
    printf("\ttest_audio_midpoint_dc_removed()...\n");

    sine_lookup_init();

    const WaveformConfig cfg = {
        .rise_pct = 0.5f,
        .fall_pct = 0.5f,
        .rise_shape = WAVE_SINE,
        .fall_shape = WAVE_SINE,
        .max_output = 350.0f,
        .midpoint = 0.001f
    };

    AudioDcBlockState dc_state = { 0.0f, 0.0f };
    double sum = 0.0;
    float phase = 0.0f;
    const int total = 4 * 12000;

    for (int i = 0; i < total; i++) {
        const float voice = (float)waveform_get_point(phase, &cfg);
        const float filtered = audio_dc_block(
            &dc_state, voice - WAVEGEN_OUTPUT_GROUND
        );
        if (i >= total / 2)
            sum += filtered;

        phase += 100.0f / 12000.0f;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    assert(fabs(sum / (total / 2)) < 1.0);
}

void test_audio_mix(void)
{
    printf("test_audio_mix()\n");
    test_center_and_clamp();
    test_click_ignores_lfo();
    test_dc_block();
    test_midpoint_dc_removed();
}
