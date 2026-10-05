#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H

#include "waveform.h"
#include "voice_manager.h"
#include "volume_oscillation.h"

#define AUDIO_SAMPLE_RATE_HZ 48000.0f
//Avoid 14-cycle float division instructions later on, use 1-3 cycle multiplication
#define AUDIO_SAMPLE_PERIOD 1.0f/AUDIO_SAMPLE_RATE_HZ

extern VoiceManager    *s_vm;
extern WaveformConfig  *s_cfg;
extern VolumeOscillationParam *s_vosc;
extern float* s_mvol;

void audio_out_init(VoiceManager *vm, WaveformConfig *cfg, VolumeOscillationParam *voscp, float *master_volume);
void set_audio_output_value(uint16_t left, uint16_t right);

/* HAL_TIM_PeriodElapsedCallback defined in audio_out.c */

#endif /* __AUDIO_OUT_H */
