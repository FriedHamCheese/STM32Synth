#include "metronome.h"
#include "metronome_click.h"

_Static_assert(
    sizeof(metronome_click) / sizeof(metronome_click[0])
        == METRONOME_CLICK_SAMPLES,
    "metronome_click.h length must match METRONOME_CLICK_SAMPLES"
);

void metronome_init(MetronomeParam *m)
{
    if (m == 0)
        return;

    m->enabled = 0;
    m->samples_to_beat = 0;
    m->sample_index = 0;
    metronome_set_bpm(m, METRONOME_DEFAULT_BPM);
}

void metronome_set_bpm(MetronomeParam *m, float bpm)
{
    if (m == 0)
        return;

    if (!(bpm >= METRONOME_MIN_BPM))
        bpm = METRONOME_MIN_BPM;
    if (bpm > METRONOME_MAX_BPM)
        bpm = METRONOME_MAX_BPM;

    m->bpm = bpm;
    m->period_samples = (uint16_t)(
        (METRONOME_SAMPLE_RATE_HZ * 60.0f / bpm) + 0.5f
    );
}

void metronome_set_enabled(MetronomeParam *m, uint8_t enabled)
{
    if (m == 0)
        return;

    const uint8_t was_enabled = m->enabled;
    m->enabled = enabled ? 1U : 0U;

    if (m->enabled && !was_enabled) {
        m->sample_index = 0;
        m->samples_to_beat = 0;
    }
}

int8_t metronome_get_sample(MetronomeParam *m)
{
    if (m == 0 || !m->enabled) {
        if (m != 0)
            m->sample_index = 0;
        return 0;
    }

    if (m->samples_to_beat > 0)
        m->samples_to_beat--;

    if (m->sample_index != 0) {
        const int8_t sample = metronome_click[m->sample_index];
        if (++m->sample_index >= METRONOME_CLICK_SAMPLES)
            m->sample_index = 0;
        return sample;
    }

    if (m->samples_to_beat > 0)
        return 0;

    m->samples_to_beat = m->period_samples;
    m->sample_index = 1;
    return metronome_click[0];
}
