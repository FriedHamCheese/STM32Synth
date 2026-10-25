#ifndef __CONTROLS_H
#define __CONTROLS_H

#include "waveform.h"
#include "volume_oscillation.h"
#include "metronome.h"

void controls_init(void);
#include "adsr.h"

void controls_init(void);
void controls_update(WaveformConfig *cfg, VolumeOscillationParam *vosc, float *master_volume, MetronomeParam *metro);
/* Tap button (SW4): call every runtime tick (10 ms) for tap-tempo timing. */
void controls_poll_tap(MetronomeParam *metro, VolumeOscillationParam *vosc);
/* Reads the ADSR pots, safe to call every loop since ADSR handles changes mid-note */
void controls_update_adsr(AdsrParam *adsr);

#endif /* __CONTROLS_H */
