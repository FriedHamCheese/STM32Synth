#ifndef VOICE_MANAGER_H
#define VOICE_MANAGER_H

#include <stdint.h>
#include "waveform.h"

#define MAX_VOICES 9
#define MAX_VOICES_INVERSE_F 1.0f/((float)MAX_VOICES)

typedef struct
{
    uint8_t active;
    uint8_t releasing;   /* 1 while ramping down to silence */
    uint16_t key_id;
    float gain;          /* de-click envelope, 0..1 */
    Waveform oscillator;
} Voice;

typedef struct
{
    Voice voices[MAX_VOICES];
    float sample_rate_hz;
    float max_frequency_hz;
    float gain_step;     /* per-callback envelope increment */
    uint8_t active_voice_count;
} VoiceManager;

void voice_manager_init(VoiceManager *manager, float sample_rate_hz, float max_frequency_hz);

uint8_t voice_manager_note_on(VoiceManager *manager, uint16_t key_id, uint16_t velocity);

void voice_manager_note_off(VoiceManager *manager, uint16_t key_id);

wavegen_output_t voice_manager_get_sample(VoiceManager *manager, const WaveformConfig *config);
float get_key_frequency(uint8_t key_id);
#endif
