/* Musical/behavioural tests for the voice manager: single notes, intervals,
 * chords and their octaves, polyphony limits, note release and the de-click
 * envelope.  Runs as part of the host (gcc) assertion suite.
 */
#include "voice_manager.h"
#include "waveform.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* Keys are absolute: get_key_frequency() maps key 0 -> C1, so C4 = 36. */
enum {
    C4 = 36,        /* C  */
    Ds4 = 39,       /* E-flat */
    E4 = 40,        /* E  */
    G4 = 43,        /* G  */
    C5 = 48         /* C, one octave up */
};

#define MIDPOINT  175.0f
#define OUT_MIN   0.0f
#define OUT_MAX   350.0f

static WaveformConfig make_config(WaveShape shape)
{
    WaveformConfig cfg = {
        .rise_pct   = 0.5f,
        .fall_pct   = 0.5f,
        .rise_shape = shape,
        .fall_shape = shape,
        .max_output = OUT_MAX
    };
    return cfg;
}

/* RMS of the mix (relative to the midpoint) over n samples. */
static float rms_for(VoiceManager *vm, const WaveformConfig *cfg, int n)
{
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        double d = (double)voice_manager_get_sample(vm, cfg) - MIDPOINT;
        acc += d * d;
    }
    return (float)sqrt(acc / (double)n);
}

/* ---- pitch / key mapping ------------------------------------------------ */

static void test_key_frequency_octaves(void)
{
    printf("\ttest_key_frequency_octaves()...\n");
    const uint8_t keys[] = { C4, E4, G4, C5 };
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        float f0 = get_key_frequency(keys[i]);
        float f1 = get_key_frequency((uint8_t)(keys[i] + 12));
        assert(f0 > 0.0f);
        assert(fabsf(f1 - 2.0f * f0) <= 0.01f * f0);   /* +12 keys ~ one octave */
    }
    assert(fabsf(get_key_frequency(C4) - 261.626f) < 0.5f);   /* C4 reference */
}

static void test_key_frequency_top_octave_not_silent(void)
{
    printf("\ttest_key_frequency_top_octave_not_silent()...\n");
    /* Regression: the octave multiplier overflowed a uint8_t for key >= 84. */
    for (uint8_t k = 84; k <= 95; k++)
        assert(get_key_frequency(k) > 0.0f);
    assert(fabsf(get_key_frequency(84) - 2.0f * get_key_frequency(72)) < 1.0f);
}

/* ---- single notes, intervals, chords ------------------------------------ */

static void test_single_note_C(void)
{
    printf("\ttest_single_note_C()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);

    assert(vm.active_voice_count == 0);
    assert(voice_manager_note_on(&vm, C4, 127) == 1);
    assert(vm.active_voice_count == 1);
    assert(rms_for(&vm, &cfg, 512) > 1.0f);         /* actually sounding */
}

static void test_interval_C_E(void)
{
    printf("\ttest_interval_C_E()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);

    assert(voice_manager_note_on(&vm, C4, 127));
    assert(voice_manager_note_on(&vm, E4, 127));
    assert(vm.active_voice_count == 2);
    assert(rms_for(&vm, &cfg, 512) > 1.0f);
}

static void test_chord_C_major(void)
{
    printf("\ttest_chord_C_major()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);

    assert(voice_manager_note_on(&vm, C4, 127));
    assert(voice_manager_note_on(&vm, E4, 127));
    assert(voice_manager_note_on(&vm, G4, 127));
    assert(vm.active_voice_count == 3);
    assert(rms_for(&vm, &cfg, 1024) > 1.0f);
}

static void test_chord_C_minor(void)
{
    printf("\ttest_chord_C_minor()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_TRIANGLE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);

    assert(voice_manager_note_on(&vm, C4, 127));
    assert(voice_manager_note_on(&vm, Ds4, 127));   /* E-flat */
    assert(voice_manager_note_on(&vm, G4, 127));
    assert(vm.active_voice_count == 3);
    assert(rms_for(&vm, &cfg, 1024) > 1.0f);
}

static void test_chord_plus_root_octave(void)
{
    printf("\ttest_chord_plus_root_octave()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);

    assert(voice_manager_note_on(&vm, C4, 127));
    assert(voice_manager_note_on(&vm, E4, 127));
    assert(voice_manager_note_on(&vm, G4, 127));
    assert(voice_manager_note_on(&vm, C5, 127));    /* root, one octave up */
    assert(vm.active_voice_count == 4);
    assert(rms_for(&vm, &cfg, 1024) > 1.0f);
    assert(voice_manager_note_on(&vm, C5, 127) == 0);   /* duplicate ignored */
    assert(vm.active_voice_count == 4);
}

/* ---- chromatic scale ---------------------------------------------------- */

static void test_chromatic_scale_pitches(void)
{
    printf("\ttest_chromatic_scale_pitches()...\n");
    /* C4..C5: twelve equal-tempered semitone steps. */
    const float semitone = 1.05946309f;   /* 2^(1/12) */
    float prev = get_key_frequency(C4);
    for (uint8_t k = C4 + 1; k <= C5; k++) {
        float f = get_key_frequency(k);
        assert(f > prev);                                 /* ascending */
        assert(fabsf(f / prev - semitone) < 0.002f);      /* ~one semitone */
        prev = f;
    }
    /* twelve steps = one octave: C5 / C4 == 2 */
    assert(fabsf(get_key_frequency(C5) - 2.0f * get_key_frequency(C4)) < 1.0f);
}

static void test_chromatic_scale_playback(void)
{
    printf("\ttest_chromatic_scale_playback()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);

    for (uint8_t k = C4; k <= C5; k++) {
        assert(voice_manager_note_on(&vm, k, 127) == 1);
        assert(vm.active_voice_count == 1);        /* previous note released */
        assert(rms_for(&vm, &cfg, 512) > 1.0f);    /* sounding */
        voice_manager_note_off(&vm, k);
        rms_for(&vm, &cfg, 512);                   /* let the release finish */
        assert(vm.active_voice_count == 0);
    }
}

/* ---- polyphony, release, de-click --------------------------------------- */

static void test_polyphony_limit(void)
{
    printf("\ttest_polyphony_limit()...\n");
    VoiceManager vm;
    voice_manager_init(&vm, 48000.0f, 48000.0f);
    for (uint8_t k = C4; k < (uint8_t)(C4 + MAX_VOICES); k++)
        assert(voice_manager_note_on(&vm, k, 127) == 1);
    assert(vm.active_voice_count == MAX_VOICES);
    assert(voice_manager_note_on(&vm, (uint8_t)(C4 + MAX_VOICES), 127) == 0);
}

static void test_duplicate_note_ignored(void)
{
    printf("\ttest_duplicate_note_ignored()...\n");
    VoiceManager vm;
    voice_manager_init(&vm, 48000.0f, 48000.0f);
    assert(voice_manager_note_on(&vm, C4, 127) == 1);
    assert(voice_manager_note_on(&vm, C4, 127) == 0);
    assert(vm.active_voice_count == 1);
}

static void test_release_frees_voice_after_ramp(void)
{
    printf("\ttest_release_frees_voice_after_ramp()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);
    assert(voice_manager_note_on(&vm, C4, 127));
    rms_for(&vm, &cfg, 256);                 /* let the attack finish */
    voice_manager_note_off(&vm, C4);
    assert(vm.active_voice_count == 1);      /* still ramping down */
    rms_for(&vm, &cfg, 2048);                /* longer than the release */
    assert(vm.active_voice_count == 0);      /* freed when the ramp ends */
}

static void test_declick_first_sample_is_midpoint(void)
{
    printf("\ttest_declick_first_sample_is_midpoint()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);
    assert(voice_manager_note_on(&vm, C4, 127));
    /* gain starts at 0, so the first sample has not moved -> no step/click */
    assert(voice_manager_get_sample(&vm, &cfg) == (wavegen_output_t)MIDPOINT);
}

static void test_output_stays_within_rails(void)
{
    printf("\ttest_output_stays_within_rails()...\n");
    VoiceManager vm;
    WaveformConfig cfg = make_config(WAVE_SINE);
    voice_manager_init(&vm, 48000.0f, 48000.0f);
    for (uint8_t k = C4; k < (uint8_t)(C4 + MAX_VOICES); k++)
        assert(voice_manager_note_on(&vm, k, 127));
    for (int i = 0; i < 48000; i++) {
        wavegen_output_t s = voice_manager_get_sample(&vm, &cfg);
        assert(s >= (wavegen_output_t)OUT_MIN && s <= (wavegen_output_t)OUT_MAX);
    }
}

/* ---- runner ------------------------------------------------------------- */

void test_musical(void)
{
    printf("test_musical()\n");
    test_key_frequency_octaves();
    test_key_frequency_top_octave_not_silent();
    test_single_note_C();
    test_interval_C_E();
    test_chord_C_major();
    test_chord_C_minor();
    test_chord_plus_root_octave();
    test_chromatic_scale_pitches();
    test_chromatic_scale_playback();
    test_polyphony_limit();
    test_duplicate_note_ignored();
    test_release_frees_voice_after_ramp();
    test_declick_first_sample_is_midpoint();
    test_output_stays_within_rails();
}
