#include "audio_out.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "volume_oscillation.h"

VoiceManager    *s_vm  = 0;
WaveformConfig  *s_cfg = 0;
VolumeOscillationParam *s_vosc = 0;

/* ponytail: file-scope pointers set once at init; safe because single-core M4 */
void audio_out_init(VoiceManager *vm, WaveformConfig *cfg, VolumeOscillationParam *voscp)
{
    s_vm  = vm;
    s_cfg = cfg;
    s_vosc = voscp;
}

//Function for readability, used only here, O1+ will probably inline this, but just to make it clear
__attribute__((always_inline))
inline void set_audio_output_value(wavegen_output_t left, wavegen_output_t right)
{
    TIM2->CCR1 = right;
    TIM2->CCR2 = left;
}

/* 48kHz ISR — called from TIM4 overflow */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM4 || !s_vm || !s_cfg || !s_vosc)
        return;

    /* ponytail: still using waveform_get_point stub; swap for voice_manager_get_sample
       once Mind wires VoiceManager into the audio path */
    const float volume = get_output_amplitude_multiplier(s_vosc);
    const float sample_midpoint = 175.0f;
    const float amplitude = (float)voice_manager_get_sample(s_vm, s_cfg) - sample_midpoint;
    const float sample = (amplitude * volume) + sample_midpoint;
    set_audio_output_value(sample, sample);
    
    get_oscillation_completion(s_vosc, AUDIO_SAMPLE_PERIOD);
}