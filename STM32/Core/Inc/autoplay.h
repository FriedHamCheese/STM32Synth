#ifndef AUTOPLAY_H
#define AUTOPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "voice_manager.h"

#define AUTOPLAY_MAX_KEYS 5
extern uint8_t autoplay_disabled_key;
extern uint8_t autoplay_max_velocity;

typedef struct
{
    uint8_t key_ids[AUTOPLAY_MAX_KEYS];
    uint16_t duration_ms;
} AutoplayStep;

typedef struct
{
    const AutoplayStep *steps;
    uint32_t step_started_ms;
    uint8_t length;
    uint8_t index;
    bool playing;
} Autoplay;

enum AutoplayKey{
  C0 = 0, Cs0, D0, Ds0, E0, F0, Fs0, G0, Gs0, A0, As0, B0,
  C1, Cs1, D1, Ds1, E1, F1, Fs1, G1, Gs1, A1, As1, B1,
  C2, Cs2, D2, Ds2, E2, F2, Fs2, G2, Gs2, A2, As2, B2,
  C3, Cs3, D3, Ds3, E3, F3, Fs3, G3, Gs3, A3, As3, B3,
  C4, Cs4, D4, Ds4, E4, F4, Fs4, G4, Gs4, A4, As4, B4,
  C5, Cs5, D5, Ds5, E5, F5, Fs5, G5, Gs5, A5, As5, B5,
  C6, Cs6, D6, Ds6, E6, F6, Fs6, G6, Gs6, A6, As6, B6,
  C7, Cs7, D7, Ds7, E7, F7, Fs7, G7, Gs7, A7,
  Key_off = 255
};

extern const AutoplayStep virtinst[];

void autoplay_start(Autoplay *autoplay, VoiceManager *voice_manager, uint32_t now_ms);
void autoplay_stop(Autoplay *autoplay, VoiceManager *voice_manager);
void autoplay_update(Autoplay *autoplay, VoiceManager *voice_manager, uint32_t now_ms);

#endif
