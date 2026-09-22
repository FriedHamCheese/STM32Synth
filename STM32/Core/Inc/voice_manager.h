#ifndef VOICE_MANAGER_H
#define VOICE_MANAGER_H

#include <stdint.h>
#include "waveform.h"

#define MAX_VOICES 9

typedef struct
{
    uint8_t active;
    uint16_t key_id;
    Waveform oscillator;
} Voice;

typedef struct
{
    Voice voices[MAX_VOICES];
    float sample_rate_hz;
    float max_frequency_hz;
    uint8_t active_voice_count;
} VoiceManager;

void voice_manager_init(VoiceManager *manager, float sample_rate_hz, float max_frequency_hz);

uint8_t voice_manager_note_on(VoiceManager *manager, uint16_t key_id, float frequency_hz);

void voice_manager_note_off(VoiceManager *manager, uint16_t key_id);

wavegen_output_t voice_manager_get_sample(VoiceManager *manager, const WaveformConfig *config);

#endif
