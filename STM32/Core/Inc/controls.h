#ifndef __CONTROLS_H
#define __CONTROLS_H

#include "waveform.h"
#include "volume_oscillation.h"

void controls_init(void);
void controls_update(WaveformConfig *cfg, VolumeOscillationParam *vosc, float *master_volume);

#endif /* __CONTROLS_H */
