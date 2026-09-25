#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H

#include "waveform.h"
#include "voice_manager.h"

#define AUDIO_SAMPLE_RATE_HZ 48000.0f

void audio_out_init(VoiceManager *vm, WaveformConfig *cfg);
void set_audio_output_value(uint16_t left, uint16_t right);

/* HAL_TIM_PeriodElapsedCallback defined in audio_out.c */

#endif /* __AUDIO_OUT_H */
