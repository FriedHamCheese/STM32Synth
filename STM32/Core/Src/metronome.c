#include "metronome.h"
#include "audio_out.h"
#include "metronome_click.h"

_Static_assert(
    METRONOME_SAMPLE_RATE_HZ == (unsigned)AUDIO_SAMPLE_RATE_HZ,
    "metronome sample rate must match the audio ISR"
);

_Static_assert(
    METRONOME_CLICK_SAMPLES < (720000U / (unsigned)METRONOME_MAX_BPM),
    "click must be shorter than the fastest beat interval"
);

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
    m->tap_active = 0;
    m->tap_has_last = 0;
    m->tap_count = 0;
    m->tap_next = 0;
    m->tap_last_ms = 0;
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

    if (enabled && !was_enabled) {
        m->sample_index = 0;
        m->samples_to_beat = 0;
    }

    m->enabled = enabled ? 1U : 0U;
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

void metronome_resync(MetronomeParam *m)
{
    if (m == 0)
        return;

    m->samples_to_beat = 0;
    m->sample_index = 0;
}

uint8_t metronome_tap(MetronomeParam *m, uint32_t now_ms)
{
    if (m == 0)
        return 0;

    if (m->tap_has_last) {
        const uint32_t interval = now_ms - m->tap_last_ms;

        if (interval < METRONOME_TAP_MIN_MS)
            return 0; /* contact bounce */

        if (interval <= METRONOME_TAP_MAX_MS) {
            m->tap_intervals[m->tap_next] = interval;
            m->tap_next = (m->tap_next + 1U) % METRONOME_TAP_INTERVALS;
            if (m->tap_count < METRONOME_TAP_INTERVALS)
                m->tap_count++;
        } else {
            /* gap out of range: this tap starts a new sequence */
            m->tap_count = 0;
            m->tap_next = 0;
        }
    }

    m->tap_last_ms = now_ms;
    m->tap_has_last = 1;
    m->tap_active = 1;

    if (m->tap_count == 0)
        return 0;

    uint32_t sum_ms = 0;
    for (uint8_t i = 0; i < m->tap_count; i++)
        sum_ms += m->tap_intervals[i];

    metronome_set_bpm(
        m, 60000.0f / ((float)sum_ms / (float)m->tap_count)
    );
    metronome_resync(m);
    return 1;
}

void metronome_tap_release(MetronomeParam *m)
{
    if (m == 0)
        return;

    m->tap_active = 0;
    m->tap_has_last = 0;
    m->tap_count = 0;
    m->tap_next = 0;
}

float metronome_get_bpm(const MetronomeParam *m)
{
    return (m != 0) ? m->bpm : 0.0f;
}
