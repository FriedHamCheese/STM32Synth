/* End-to-end (host) check of the tap -> metronome -> LFO chain. */
#include "metronome.h"
#include "volume_oscillation.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_tap_click_spacing(void)
{
    printf("\ttest_tap_click_spacing()...\n");

    MetronomeParam m;
    metronome_init(&m);
    metronome_set_enabled(&m, 1);

    assert(metronome_tap(&m, 0) == 0);
    assert(metronome_tap(&m, 500) == 1);   /* 500 ms taps -> 120 BPM */
    assert(m.period_samples == 6000);      /* 12000 Hz * 60 / 120 */

    unsigned clicks[3];
    unsigned found = 0;
    for (unsigned i = 0; i < METRONOME_SAMPLE_RATE_HZ * 2U && found < 3; i++) {
        const uint16_t before = m.sample_index;
        metronome_get_sample(&m);
        if (before == 0 && m.sample_index != 0)
            clicks[found++] = i;
    }

    assert(found == 3);
    assert(clicks[1] - clicks[0] == 6000);
    assert(clicks[2] - clicks[1] == 6000);
}

static void test_tap_lfo_phase(void)
{
    printf("\ttest_tap_lfo_phase()...\n");

    MetronomeParam m;
    metronome_init(&m);
    assert(metronome_tap(&m, 0) == 0);
    assert(metronome_tap(&m, 500) == 1);

    /* mid pot = 1/2 cycle per beat: 1 Hz at 120 BPM */
    VolumeOscillationParam vosc = {
        .frequency = vosc_beat_sync_hz(metronome_get_bpm(&m), 0.5f),
        .completion = 0.0f,
        .strength = 1.0f
    };
    assert(fabsf(vosc.frequency - 1.0f) < 0.01f);

    /* half a second is half an LFO cycle */
    for (unsigned i = 0; i < METRONOME_SAMPLE_RATE_HZ / 2U; i++)
        get_oscillation_completion(&vosc, 1.0f / METRONOME_SAMPLE_RATE_HZ);

    assert(fabsf(vosc.completion - 0.5f) < 0.001f);
}

void test_tap_tempo_sync(void)
{
    printf("test_tap_tempo_sync()\n");
    test_tap_click_spacing();
    test_tap_lfo_phase();
}
