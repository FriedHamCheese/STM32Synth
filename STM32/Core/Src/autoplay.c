#include "autoplay.h"
#include <stddef.h>

uint8_t autoplay_disabled_key = UINT8_MAX;
uint8_t autoplay_max_velocity = 127;

static void note_on_step(VoiceManager *vm, const AutoplayStep *step)
{
    for (uint8_t i = 0; i < AUTOPLAY_MAX_KEYS; ++i)
    {
        if (step->key_ids[i] != autoplay_disabled_key)
            voice_manager_note_on(vm, step->key_ids[i], autoplay_max_velocity);
    }
}

static void note_off_step(VoiceManager *vm, const AutoplayStep *step)
{
    for (uint8_t i = 0; i < AUTOPLAY_MAX_KEYS; ++i)
    {
        if (step->key_ids[i] != autoplay_disabled_key)
            voice_manager_note_off(vm, step->key_ids[i]);
    }
}

void autoplay_start(Autoplay *autoplay, VoiceManager *vm, uint32_t now_ms)
{
    if (autoplay == 0 || vm == 0 || autoplay->steps == 0 || autoplay->length == 0)
        return;

    const uint8_t stop_ongoing_keys = autoplay->playing;
    if (stop_ongoing_keys)
        note_off_step(vm, &autoplay->steps[autoplay->index]);

    autoplay->index = 0;
    autoplay->step_started_ms = now_ms;
    autoplay->playing = true;
    note_on_step(vm, &(autoplay->steps[0]));
}

void autoplay_stop(Autoplay *autoplay, VoiceManager *vm)
{
    if (!autoplay || !vm || (autoplay->steps == NULL)) 
      return;
    if (autoplay->playing)
      note_off_step(vm, &(autoplay->steps[autoplay->index]));
    autoplay->playing = false;
}

void autoplay_update(Autoplay *autoplay, VoiceManager *vm, uint32_t now_ms)
{
    if (!autoplay || !vm || autoplay->steps == NULL || autoplay->length == 0 || !(autoplay->playing))
        return;

    const AutoplayStep *current = &autoplay->steps[autoplay->index];
    const uint8_t last_step_ended = (now_ms - ((uint32_t)autoplay->step_started_ms)) >= current->duration_ms;
    if (!last_step_ended)
        return;

    note_off_step(vm, &(autoplay->steps[autoplay->index]));
    
    autoplay->index++;
    const uint8_t wrap_to_beginning = autoplay->index >= autoplay->length;
    if (wrap_to_beginning)
        autoplay->index = 0;
      
    note_on_step(vm, &(autoplay->steps[autoplay->index]));
    autoplay->step_started_ms = now_ms;
}


const AutoplayStep virtinst[] = {
  { { Ds2, Ds3, Key_off, Key_off, Key_off }, 250 },
  { { Ds3, As3, Cs4, Ds4, Fs4 }, 1000 },
  { { As1, As2, Key_off, Key_off, Key_off }, 250 },
  { { As2, As3, C4, Ds4, Fs4 }, 1000 },
  { { Cs2, Cs3, Key_off, Key_off, Key_off }, 250 },
  { { Cs3, B3, Ds4, F4, Gs4 }, 1000 },
  { { Fs1, Fs2, Key_off, Key_off, Key_off }, 250 },
  { { Fs2, As3, Cs4, F4, Fs4 }, 1000 },
  { { C2, C3, Key_off, Key_off, Key_off }, 250 },
  { { C3, As3, C4, Ds4, Fs4 }, 1000 },
  { { B1, B2, Key_off, Key_off, Key_off }, 250 },
  { { B2, As3, B3, Ds4, Fs4 }, 1000 },
  { { As1, As2, Key_off, Key_off, Key_off }, 250 },
  { { As2, Gs3, As3, D4, Fs4 }, 1000 },
  { { Ds2, Ds3, Key_off, Key_off, Key_off }, 250 },
  { { Ds3, As3, Cs4, Ds4, Fs4 }, 5000 }
};