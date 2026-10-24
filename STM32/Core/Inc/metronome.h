#ifndef METRONOME_H
#define METRONOME_H

#include <stdint.h>

#define METRONOME_MIN_BPM 180.0f
#define METRONOME_MAX_BPM 240.0f
#define METRONOME_DEFAULT_BPM 200.0f
#define METRONOME_SAMPLE_RATE_HZ 12000U
#define METRONOME_CLICK_SAMPLES 360U

typedef struct {
    float bpm;
    volatile uint8_t enabled;
    volatile uint16_t period_samples;
    volatile uint16_t samples_to_beat;
    volatile uint16_t sample_index;
} MetronomeParam;

void metronome_init(MetronomeParam *m);
void metronome_set_bpm(MetronomeParam *m, float bpm);
void metronome_set_enabled(MetronomeParam *m, uint8_t enabled);
int8_t metronome_get_sample(MetronomeParam *m);

#endif
