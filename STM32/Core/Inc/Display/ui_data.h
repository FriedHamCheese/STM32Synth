/*
 * ui_data.h  —  the values the screen shows.
 *
 * DECOUPLED: the screen code never includes any synth module. The firmware
 * owns the data flow — it fills the global `ui_data` (e.g. in its own
 * update_ui_data()) and then calls ui_task(). The waveform is passed in as a
 * function pointer, so the screen doesn't know which module produces it.
 *
 *     static void update_ui_data(void) {
 *         ui_data.wave_sample = waveform_get_point_as_amplitude;
 *         ui_data.rise_pct    = waveform_config.rise_pct * 100.0f;
 *         ...
 *     }
 *     while (1) { update_ui_data(); ui_task(); }
 *
 * Every field defaults to zero/false/NULL (it's a global), and anything with a
 * *_valid flag shows "--" until the firmware sets that flag. So new features
 * can be wired in one at a time.
 *
 * ui_task() takes a copy of ui_data at the start of each frame, so changing
 * fields while a frame is being drawn can't tear a single panel.
 */
#ifndef UI_DATA_H
#define UI_DATA_H

#include <stdint.h>
#include <stdbool.h>

/* phase 0.0 .. 1.0  ->  amplitude -1.0 .. +1.0 */
typedef float (*ui_wave_fn)(float phase);

typedef struct {
    /* ---- waveform ---------------------------------------------------- */
    ui_wave_fn wave_sample;     /* NULL -> flat line                         */
    float rise_pct;             /* 0 .. 100                                  */
    float fall_pct;             /* 0 .. 100                                  */

    /* ---- ADSR: A/D/R in seconds (0.1 .. 10), S level 0.0 .. 1.0 x ------ */
    bool  adsr_valid;
    float attack_s;
    float decay_s;
    float sustain;
    float release_s;

    /* ---- volume oscillator ------------------------------------------- */
    float lfo_depth;            /* strength 0.0 .. 1.0                       */
    float lfo_hz;               /* 0.1 .. 100 Hz                             */

    /* ---- output: 0.0 .. 10.0 x, above 1.0 = overdrive ---------------- */
    bool  volume_valid;
    float volume;

    /* ================= v2 ============================================= */

    /* Autoplay song name. NULL or "" -> header shows "MANUAL PLAY".
     * Must point to memory that stays valid (e.g. the song list labels). */
    const char *autoplay_label;

    /* Wave centre oscillator: 0.0 = off, up to 20 Hz */
    bool  wave_center_osc_valid;
    float wave_center_osc_hz;

    /* Filter. LPF and HPF share one knob: at most one is non-zero,
     * 0 = off (if both are non-zero the screen shows the LPF).
     * ADSR and fixed strength share one knob the same way, 0.0 .. 1.0. */
    bool  filter_valid;
    float lowpass_cutoff_hz;
    float highpass_cutoff_hz;
    float filter_adsr_strength;
    float filter_fixed_strength;
} ui_data_t;

/* Defined in ui.c. The firmware writes it, the screen reads it. */
extern ui_data_t ui_data;

/* ---- ui_port.c ---------------------------------------------------------- */

/* Milliseconds since boot (HAL_GetTick on target). */
uint32_t ui_port_millis(void);

/* Fake animated values for bring-up without the synth code (and for the PC
 * preview). Call instead of update_ui_data():  ui_demo_update(&ui_data);  */
void ui_demo_update(ui_data_t *d);

#endif /* UI_DATA_H */
