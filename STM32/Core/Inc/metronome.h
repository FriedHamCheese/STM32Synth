#ifndef METRONOME_H
#define METRONOME_H

#include <stdint.h>

#define METRONOME_MIN_BPM 40.0f
#define METRONOME_MAX_BPM 300.0f
#define METRONOME_DEFAULT_BPM 120.0f
#define METRONOME_SAMPLE_RATE_HZ 12000U
#define METRONOME_CLICK_SAMPLES 360U

/* Tap tempo: accepted interval range, sequence timeout, intervals averaged. */
#define METRONOME_TAP_MIN_MS 200U
#define METRONOME_TAP_MAX_MS 1500U
#define METRONOME_TAP_TIMEOUT_MS 2000U
#define METRONOME_TAP_INTERVALS 4U

typedef struct {
    float bpm;
    volatile uint8_t enabled;
    volatile uint16_t period_samples;
    volatile uint16_t samples_to_beat;
    volatile uint16_t sample_index;
    uint8_t tap_active;
    uint8_t tap_has_last;
    uint8_t tap_count;
    uint8_t tap_next;
    uint32_t tap_last_ms;
    uint32_t tap_intervals[METRONOME_TAP_INTERVALS];
} MetronomeParam;

void metronome_init(MetronomeParam *m);
void metronome_set_bpm(MetronomeParam *m, float bpm);
void metronome_set_enabled(MetronomeParam *m, uint8_t enabled);
int8_t metronome_get_sample(MetronomeParam *m);

/* Restart the beat grid so the next sample is a click (tap lands on the beat). */
void metronome_resync(MetronomeParam *m);

/* Report a tap at now_ms. Returns 1 when the tapped tempo was applied. */
uint8_t metronome_tap(MetronomeParam *m, uint32_t now_ms);

/* Hand tempo back to the BPM pot. */
void metronome_tap_release(MetronomeParam *m);

float metronome_get_bpm(const MetronomeParam *m);

#endif
