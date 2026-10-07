#include "voice_manager.h"
#include <math.h>

#define OUTPUT_MIDPOINT 175.0f
#define OUTPUT_MIN 0.0f
#define OUTPUT_MAX 350.0f

//note_on/note_off run in the main loop while get_sample runs in the 48kHz TIM4 ISR,
//block the ISR while a voice is being changed so it never sees a half-written voice.
#ifdef PC_UNITTEST
#define VOICE_LOCK()
#define VOICE_UNLOCK()
#else
#include "stm32f4xx_hal.h"
#define VOICE_LOCK()   uint32_t primask_ = __get_PRIMASK(); __disable_irq()
#define VOICE_UNLOCK() __set_PRIMASK(primask_)
#endif

void voice_manager_init(
    VoiceManager *manager,
    float sample_rate_hz,
    float max_frequency_hz,
    const AdsrParam *adsr_param,
    uint32_t (*get_ms)(void)
){
    if (manager == 0)
        return;

    manager->sample_rate_hz = sample_rate_hz;
    manager->max_frequency_hz = max_frequency_hz;
    manager->active_voice_count = 0;
    manager->adsr_param = adsr_param;
    manager->get_ms = get_ms;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        manager->voices[i].active = 0;
        manager->voices[i].key_id = 0;
        manager->voices[i].oscillator.waveform_completion_ratio = 0.0f;
        manager->voices[i].oscillator.waveform_completion_increment = 0.0f;
        adsr_init(&manager->voices[i].envelope);
    }
}

uint8_t voice_manager_note_on(VoiceManager *manager, uint16_t key_id, uint16_t velocity)
{
    uint8_t started = 0;

    if (manager == 0 ||
        manager->get_ms == 0 ||
        manager->sample_rate_hz <= 0.0f ||
        manager->max_frequency_hz <= 0.0f
    )
        return 0;

    float frequency_hz = get_key_frequency(key_id);
    if (frequency_hz > manager->max_frequency_hz)
        frequency_hz = manager->max_frequency_hz;

    VOICE_LOCK();
    const uint32_t now_ms = manager->get_ms();

    //Key still has a voice (it's releasing), retrigger attack from its current volume
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (manager->voices[i].active && manager->voices[i].key_id == key_id)
        {
            adsr_note_on(&manager->voices[i].envelope, manager->adsr_param, now_ms);
            started = 1;
            break;
        }
    }

    for (int i = 0; i < MAX_VOICES && !started; i++)
    {
        if (!manager->voices[i].active)
        {
            manager->voices[i].key_id = key_id;
            manager->voices[i].oscillator.waveform_completion_ratio = 0.0f;
            manager->voices[i].oscillator.waveform_completion_increment =
                frequency_hz / manager->sample_rate_hz;
            adsr_init(&manager->voices[i].envelope);
            adsr_note_on(&manager->voices[i].envelope, manager->adsr_param, now_ms);
            manager->voices[i].active = 1;
            started = 1;
        }
    }
    VOICE_UNLOCK();

    return started;
}

void voice_manager_note_off(VoiceManager *manager, uint16_t key_id)
{
    if (manager == 0 || manager->get_ms == 0)
        return;

    VOICE_LOCK();
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (manager->voices[i].active && manager->voices[i].key_id == key_id)
        {
            //Voice stays active until release finishes, get_sample frees it
            adsr_note_off(&manager->voices[i].envelope, manager->adsr_param, manager->get_ms());
            break;
        }
    }
    VOICE_UNLOCK();
}

wavegen_output_t voice_manager_get_sample(VoiceManager *manager, const WaveformConfig *config)
{
    float mixed_sample = 0.0f;
    float sample;
    float phase;
    float volume;
    uint8_t voice_count = 0;

    if (manager == 0 || config == 0 || manager->get_ms == 0)
        return (wavegen_output_t)OUTPUT_MIDPOINT;

    const uint32_t now_ms = manager->get_ms();

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (manager->voices[i].active)
        {
            volume = adsr_get_volume(&manager->voices[i].envelope, manager->adsr_param, now_ms);

            if (!adsr_is_active(&manager->voices[i].envelope))
            {
                manager->voices[i].active = 0;
                continue;
            }

            phase = manager->voices[i].oscillator.waveform_completion_ratio;

            sample = (float)adsr_apply_volume(waveform_get_point(phase, config), volume);

            mixed_sample += sample - OUTPUT_MIDPOINT;

            phase += manager->voices[i].oscillator.waveform_completion_increment;

            if (phase >= 1.0f)
                phase -= 1.0f;

            manager->voices[i].oscillator.waveform_completion_ratio = phase;

            voice_count++;
        }
    }

    manager->active_voice_count = voice_count;

    if (voice_count == 0)
        return (wavegen_output_t)OUTPUT_MIDPOINT;

    mixed_sample *= MAX_VOICES_INVERSE_F;
    mixed_sample += OUTPUT_MIDPOINT;

    if (mixed_sample < OUTPUT_MIN)
        mixed_sample = OUTPUT_MIN;

    if (mixed_sample > OUTPUT_MAX)
        mixed_sample = OUTPUT_MAX;

    return (wavegen_output_t)mixed_sample;
}

static float frequencies_at_octave_0[12] = {
  16.35160f,
  17.32391f,
  18.35405f,
  19.44544f,
  20.60172f,
  21.82676f,
  23.12465f,
  24.49971f,
  25.95654f,
  27.50000f,
  29.13524f,
  30.86771f
};

float get_key_frequency(uint8_t key_id){
  const uint8_t keys_per_octave = 12;
  const uint8_t octave_from_octave_0 = key_id / keys_per_octave;
  const uint8_t key = key_id % keys_per_octave;
  
  //An octave higher is 2x the frequency
  const uint8_t octave_frequency_multiplier = 1 << (octave_from_octave_0 + 1);
  return frequencies_at_octave_0[key] * (float)octave_frequency_multiplier;
} 
