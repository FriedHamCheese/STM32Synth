#ifndef __CONTROLS_H
#define __CONTROLS_H

#include "waveform.h"
#include "adsr.h"

void controls_init(void);
void controls_update(WaveformConfig *cfg);
/* Reads the ADSR pots, safe to call every loop since ADSR handles changes mid-note */
void controls_update_adsr(AdsrParam *adsr);

#endif /* __CONTROLS_H */
