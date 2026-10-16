/*
 * ui_data.h  —  the values the screen shows, and where they come from.
 *
 * The UI never touches the synth's globals directly. main.c supplies a
 * snapshot once per frame, and the display port renders from that snapshot.
 */
#ifndef UI_DATA_H
#define UI_DATA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* Synth-owned waveform sampler used by the waveform panel. */
    float (*wave_sample)(float phase);

    /* Global waveform shape: rise/fall split in percent (rise + fall = 100) */
    float rise_pct;
    float fall_pct;

    /* ADSR. A/D/R are times in seconds (0.1 .. 10.0),
     * S is a level, 0.0 .. 1.0 x volume.                                   */
    bool  adsr_valid;           /* false -> panel shows "--" (not built yet) */
    float attack_s;
    float decay_s;
    float sustain;
    float release_s;

    /* Volume oscillator (tremolo / volume LFO) */
    float lfo_depth;            /* strength 0.0 .. 1.0                       */
    float lfo_hz;               /* frequency 0.1 .. 100 Hz                   */

    /* Output volume, 0.0 .. 10.0 x. Above 1.0 = overdrive.                 */
    bool  volume_valid;         /* false -> panel shows "--" (not built yet) */
    float volume;
} ui_data_t;

/* Milliseconds since boot (HAL_GetTick on target). */
uint32_t ui_port_millis(void);

#endif /* UI_DATA_H */
