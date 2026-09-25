#include "audio_out.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

static VoiceManager    *s_vm  = 0;
static WaveformConfig  *s_cfg = 0;

/* ponytail: file-scope pointers set once at init; safe because single-core M4 */
void audio_out_init(VoiceManager *vm, WaveformConfig *cfg)
{
    s_vm  = vm;
    s_cfg = cfg;
}

void set_audio_output_value(uint16_t left, uint16_t right)
{
    TIM3->CCR1 = right;
    TIM3->CCR2 = left;
}

/* 48kHz ISR — called from TIM4 overflow */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM4 || !s_vm || !s_cfg)
        return;

    /* ponytail: still using waveform_get_point stub; swap for voice_manager_get_sample
       once Mind wires VoiceManager into the audio path */
    wavegen_output_t sample = voice_manager_get_sample(s_vm, s_cfg);
    set_audio_output_value(sample, sample);
}
