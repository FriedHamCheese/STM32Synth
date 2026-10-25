#include "metronome.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_beat_onsets(
    MetronomeParam *m,
    unsigned samples,
    const unsigned *expected,
    unsigned expected_count
) {
    unsigned found = 0;
    uint16_t previous_index = 0;

    for (unsigned i = 0; i < samples; i++) {
        metronome_get_sample(m);
        if (previous_index == 0 && m->sample_index != 0) {
            assert(found < expected_count);
            assert(i == expected[found]);
            found++;
        }
        previous_index = m->sample_index;
    }

    assert(found == expected_count);
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
    const unsigned fast_onsets[] = { 0, 3000, 6000, 9000 };
    assert_beat_onsets(
        &m, METRONOME_SAMPLE_RATE_HZ,
        fast_onsets, sizeof(fast_onsets)/sizeof(fast_onsets[0])
    );

    metronome_init(&m);
    metronome_set_enabled(&m, 1);
    metronome_set_bpm(&m, 180.0f);
    assert(m.period_samples == 4000);
    const unsigned slow_onsets[] = { 0, 4000, 8000 };
    assert_beat_onsets(
        &m, METRONOME_SAMPLE_RATE_HZ,
        slow_onsets, sizeof(slow_onsets)/sizeof(slow_onsets[0])
    );
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

    metronome_set_bpm(&m, 10.0f);
    assert(m.bpm == METRONOME_MIN_BPM);
    assert(m.period_samples == 18000);

    metronome_set_bpm(&m, 400.0f);
    assert(m.bpm == METRONOME_MAX_BPM);
    assert(m.period_samples == 2400);

    metronome_set_bpm(&m, NAN);
    assert(m.bpm == METRONOME_MIN_BPM);
}

static void test_tap_sets_bpm(void)
{
    printf("\ttest_metronome_tap_sets_bpm()...\n");

    MetronomeParam m;
    metronome_init(&m);

    assert(metronome_tap(&m, 0) == 0);   /* first tap only arms the sequence */
    assert(metronome_tap(&m, 500) == 1);
    assert(fabsf(m.bpm - 120.0f) < 0.5f);
    assert(m.period_samples == 6000);
    assert(m.tap_active == 1);

    /* the four-interval average stays at 120 BPM */
    assert(metronome_tap(&m, 1000) == 1);
    assert(metronome_tap(&m, 1500) == 1);
    assert(metronome_tap(&m, 2000) == 1);
    assert(fabsf(m.bpm - 120.0f) < 0.5f);
}

static void test_tap_timeout_and_release(void)
{
    printf("\ttest_metronome_tap_timeout_and_release()...\n");

    MetronomeParam m;
    metronome_init(&m);

    assert(metronome_tap(&m, 0) == 0);
    assert(metronome_tap(&m, 100) == 0);  /* bounce ignored */
    assert(metronome_tap(&m, 500) == 1);
    assert(fabsf(m.bpm - 120.0f) < 0.5f);

    /* gap beyond the timeout restarts the sequence: this tap only arms it */
    assert(metronome_tap(&m, 3000) == 0);
    assert(metronome_tap(&m, 3500) == 1);
    assert(fabsf(m.bpm - 120.0f) < 0.5f);

    metronome_tap_release(&m);
    assert(m.tap_active == 0);
    assert(metronome_tap(&m, 5000) == 0);
    assert(m.tap_active == 1);
}

static void test_tap_resyncs_beat(void)
{
    printf("\ttest_metronome_tap_resyncs_beat()...\n");

    MetronomeParam m;
    metronome_init(&m);
    metronome_set_enabled(&m, 1);
    metronome_set_bpm(&m, 300.0f);

    for (unsigned i = 0; i < 50; i++)
        metronome_get_sample(&m);
    assert(m.samples_to_beat != 0);

    metronome_tap(&m, 0);
    metronome_tap(&m, 500);

    assert(m.samples_to_beat == 0);
    assert(m.sample_index == 0);
    metronome_get_sample(&m);
    assert(m.sample_index == 1);  /* click restarted on the tap */
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
    test_tap_sets_bpm();
    test_tap_timeout_and_release();
    test_tap_resyncs_beat();
    test_enable_resets_state();
    test_null_safe();
}
