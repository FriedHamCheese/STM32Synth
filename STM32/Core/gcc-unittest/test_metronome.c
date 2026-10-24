#include "metronome.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static unsigned count_beats(MetronomeParam *m, unsigned samples)
{
    unsigned beats = 0;
    uint16_t previous_index = 0;

    for (unsigned i = 0; i < samples; i++) {
        metronome_get_sample(m);
        if (previous_index == 0 && m->sample_index != 0)
            beats++;
        previous_index = m->sample_index;
    }

    return beats;
}

static void test_disabled_is_silent(void)
{
    printf("\ttest_metronome_disabled_is_silent()...\n");

    MetronomeParam m;
    metronome_init(&m);

    for (unsigned i = 0; i < METRONOME_SAMPLE_RATE_HZ; i++)
        assert(metronome_get_sample(&m) == 0);
}

static void test_beat_intervals(void)
{
    printf("\ttest_metronome_beat_intervals()...\n");

    MetronomeParam m;
    metronome_init(&m);
    metronome_set_enabled(&m, 1);

    metronome_set_bpm(&m, 240.0f);
    assert(m.period_samples == 3000);
    assert(count_beats(&m, METRONOME_SAMPLE_RATE_HZ) == 4);

    metronome_init(&m);
    metronome_set_enabled(&m, 1);
    metronome_set_bpm(&m, 180.0f);
    assert(m.period_samples == 4000);
    assert(count_beats(&m, METRONOME_SAMPLE_RATE_HZ) == 3);
}

static void test_click_length(void)
{
    printf("\ttest_metronome_click_length()...\n");

    MetronomeParam m;
    metronome_init(&m);
    metronome_set_bpm(&m, 240.0f);
    metronome_set_enabled(&m, 1);

    metronome_get_sample(&m);
    for (unsigned i = 1; i < METRONOME_CLICK_SAMPLES; i++)
        metronome_get_sample(&m);

    assert(m.sample_index == 0);
}

static void test_bpm_clamp(void)
{
    printf("\ttest_metronome_bpm_clamp()...\n");

    MetronomeParam m;
    metronome_init(&m);

    metronome_set_bpm(&m, 100.0f);
    assert(m.bpm == METRONOME_MIN_BPM);
    assert(m.period_samples == 4000);

    metronome_set_bpm(&m, 300.0f);
    assert(m.bpm == METRONOME_MAX_BPM);
    assert(m.period_samples == 3000);

    metronome_set_bpm(&m, NAN);
    assert(m.bpm == METRONOME_MIN_BPM);
}

static void test_enable_resets_state(void)
{
    printf("\ttest_metronome_enable_resets_state()...\n");

    MetronomeParam m;
    metronome_init(&m);
    metronome_set_bpm(&m, 240.0f);
    metronome_set_enabled(&m, 1);

    for (unsigned i = 0; i < 100; i++)
        metronome_get_sample(&m);
    assert(m.sample_index != 0);

    metronome_set_enabled(&m, 0);
    assert(metronome_get_sample(&m) == 0);
    assert(m.sample_index == 0);

    metronome_set_enabled(&m, 1);
    assert(m.samples_to_beat == 0);
    assert(m.sample_index == 0);
}

static void test_null_safe(void)
{
    printf("\ttest_metronome_null_safe()...\n");

    metronome_init(0);
    metronome_set_bpm(0, 200.0f);
    metronome_set_enabled(0, 1);
    assert(metronome_get_sample(0) == 0);
}

void test_metronome(void)
{
    printf("test_metronome()\n");
    test_disabled_is_silent();
    test_beat_intervals();
    test_click_length();
    test_bpm_clamp();
    test_enable_resets_state();
    test_null_safe();
}
