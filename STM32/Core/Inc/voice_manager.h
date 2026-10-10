#ifndef VOICE_MANAGER_H
#define VOICE_MANAGER_H

#include <stdint.h>
#include "waveform.h"
#include "adsr.h"

#define MAX_VOICES 9
#define MAX_VOICES_INVERSE_F 1.0f/((float)MAX_VOICES)

typedef struct
{
    ///Set by note_on in main loop, cleared by get_sample in ISR once envelope release finishes
    uint8_t active;
    uint16_t key_id;
    Waveform oscillator;
    AdsrState envelope;
} Voice;

typedef struct
{
    Voice voices[MAX_VOICES];
    float sample_rate_hz;
    float max_frequency_hz;
    ///Voices sounding as of the last sample, including releasing ones. Only written by get_sample.
    uint8_t active_voice_count;
    ///NULL plays notes with no envelope
    const AdsrParam *adsr_param;
} VoiceManager;

void voice_manager_init(
    VoiceManager *manager,
    float sample_rate_hz,
    float max_frequency_hz,
    const AdsrParam *adsr_param
);

///Starts a voice for key_id, or retriggers its attack if the key is still releasing.
uint8_t voice_manager_note_on(VoiceManager *manager, uint16_t key_id, uint16_t velocity);

///Puts the key's voice into release, the voice is freed once release finishes.
void voice_manager_note_off(VoiceManager *manager, uint16_t key_id);

wavegen_output_t voice_manager_get_sample(VoiceManager *manager, const WaveformConfig *config);
float get_key_frequency(uint8_t key_id);
#endif
