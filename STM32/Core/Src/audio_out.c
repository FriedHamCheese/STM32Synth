#include "audio_out.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "volume_oscillation.h"
#include "main.h"

static VoiceManager    *s_vm  = 0;
static WaveformConfig  *s_cfg = 0;
static VolumeOscillationParam *s_vosc = 0;
static float* s_mvol = 0;
static MetronomeParam *s_metro = 0;

/* ponytail: file-scope pointers set once at init; safe because single-core M4 */
void audio_out_init(VoiceManager *vm, WaveformConfig *cfg, VolumeOscillationParam *voscp, float *master_volume, MetronomeParam *metro)
{
    s_vm  = vm;
    s_cfg = cfg;
    s_vosc = voscp;
    s_mvol = master_volume;
    s_metro = metro;
}

//Function for readability, used only here, O1+ will probably inline this, but just to make it clear
__attribute__((always_inline))
inline void set_audio_output_value(wavegen_output_t left, wavegen_output_t right)
{
    TIM2->CCR1 = right;
    TIM2->CCR2 = left;
}

/* 12kHz ISR — called from TIM4 overflow */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        main_runtime_iter();
        return;
    }

    if (htim->Instance != TIM4 || !s_vm || !s_cfg || !s_vosc || !s_mvol || !s_metro)
        return;

    /* ponytail: still using waveform_get_point stub; swap for voice_manager_get_sample
       once Mind wires VoiceManager into the audio path */
    const float lfo_multiplier = get_output_amplitude_multiplier(s_vosc);
    const float voice_sample = (float)voice_manager_get_sample(s_vm, s_cfg);
    const float click = (float)metronome_get_sample(s_metro);
    const float sample = audio_mix_sample(
        voice_sample, click, *s_mvol, lfo_multiplier
    );

    set_audio_output_value((wavegen_output_t)sample, (wavegen_output_t)sample);
    
    get_oscillation_completion(s_vosc, AUDIO_SAMPLE_PERIOD);
}
