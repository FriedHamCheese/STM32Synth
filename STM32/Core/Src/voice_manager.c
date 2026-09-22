#include "voice_manager.h"
#include <math.h>

#define OUTPUT_MIDPOINT 175.0f
#define OUTPUT_MIN 0.0f
#define OUTPUT_MAX 350.0f

void voice_manager_init(VoiceManager *manager, float sample_rate_hz, float max_frequency_hz)
{
    if (manager == 0)
        return;

    manager->sample_rate_hz = sample_rate_hz;
    manager->max_frequency_hz = max_frequency_hz;
    manager->active_voice_count = 0;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        manager->voices[i].active = 0;
        manager->voices[i].key_id = 0;
        manager->voices[i].oscillator.waveform_completion_ratio = 0.0f;
        manager->voices[i].oscillator.waveform_completion_increment = 0.0f;
    }
}

uint8_t voice_manager_note_on(VoiceManager *manager, uint16_t key_id, float frequency_hz)
{
    if (manager == 0 ||
        !isfinite(frequency_hz) ||
        manager->sample_rate_hz <= 0.0f ||
        manager->max_frequency_hz <= 0.0f ||
        frequency_hz <= 0.0f)
        return 0;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (manager->voices[i].active && manager->voices[i].key_id == key_id)
            return 0;
    }

    if (manager->active_voice_count >= MAX_VOICES)
        return 0;

    if (frequency_hz > manager->max_frequency_hz)
        frequency_hz = manager->max_frequency_hz;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!manager->voices[i].active)
        {
            manager->voices[i].active = 1;
            manager->voices[i].key_id = key_id;
            manager->voices[i].oscillator.waveform_completion_ratio = 0.0f;
            manager->voices[i].oscillator.waveform_completion_increment =
                frequency_hz / manager->sample_rate_hz;

            manager->active_voice_count++;

            return 1;
        }
    }

    return 0;
}

void voice_manager_note_off(VoiceManager *manager, uint16_t key_id)
{
    if (manager == 0)
        return;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (manager->voices[i].active && manager->voices[i].key_id == key_id)
        {
            manager->voices[i].active = 0;

            if (manager->active_voice_count > 0)
                manager->active_voice_count--;

            return;
        }
    }
}

wavegen_output_t voice_manager_get_sample(VoiceManager *manager, const WaveformConfig *config)
{
    float mixed_sample = 0.0f;
    float sample;
    float phase;
    uint8_t voice_count = 0;

    if (manager == 0 || config == 0 || manager->active_voice_count == 0)
        return (wavegen_output_t)OUTPUT_MIDPOINT;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (manager->voices[i].active)
        {
            phase = manager->voices[i].oscillator.waveform_completion_ratio;

            sample = (float)waveform_get_point(phase, config);

            mixed_sample += sample - OUTPUT_MIDPOINT;

            phase += manager->voices[i].oscillator.waveform_completion_increment;

            if (phase >= 1.0f)
                phase -= 1.0f;

            manager->voices[i].oscillator.waveform_completion_ratio = phase;

            voice_count++;
        }
    }

    if (voice_count == 0)
        return (wavegen_output_t)OUTPUT_MIDPOINT;

    mixed_sample /= (float)voice_count;
    mixed_sample += OUTPUT_MIDPOINT;

    if (mixed_sample < OUTPUT_MIN)
        mixed_sample = OUTPUT_MIN;

    if (mixed_sample > OUTPUT_MAX)
        mixed_sample = OUTPUT_MAX;

    return (wavegen_output_t)mixed_sample;
}

