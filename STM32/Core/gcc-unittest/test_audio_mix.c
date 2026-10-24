#include "audio_out.h"

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

void test_audio_mix(void)
{
    printf("test_audio_mix()\n");
    test_center_and_clamp();
    test_click_ignores_lfo();
}
