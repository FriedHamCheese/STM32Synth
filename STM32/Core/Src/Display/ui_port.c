/*
 * ui_port.c  —  platform glue for the screen.
 *
 * Only two things live here:
 *   - ui_port_millis(): the clock (HAL_GetTick on target)
 *   - ui_demo_update(): fake animated values, so the screen can be brought up
 *     without the synth code. Unused in the real firmware (the linker drops it
 *     with -ffunction-sections / --gc-sections, which CubeIDE uses by default).
 *
 * No synth modules are included anywhere in the screen code. The firmware
 * fills `ui_data` itself, see ui_data.h.
 */
#include "ui_data.h"
#include <math.h>
#include <stddef.h>

#ifdef UI_HOST_SIM
uint32_t sim_millis(void);                 /* provided by sim/main_sim.c */
uint32_t ui_port_millis(void) { return sim_millis(); }
#else
#include "main.h"                          /* HAL only */
uint32_t ui_port_millis(void) { return HAL_GetTick(); }
#endif

/* ======================================================================== */
/*  demo data                                                               */
/* ======================================================================== */

#define PI_F 3.14159265f

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Triangle 0..1 that takes `period_ms` for a full up-and-down cycle */
static float tri(uint32_t t, uint32_t period_ms)
{
    float p = (float)(t % period_ms) / (float)period_ms;
    return p < 0.5f ? p * 2.0f : 2.0f - p * 2.0f;
}

static float demo_rise = 0.35f;

/* Asymmetric "shark fin" with a bit of 3rd harmonic */
static float demo_wave(float phase)
{
    phase -= floorf(phase);
    float r = clampf(demo_rise, 0.02f, 0.98f);
    float v = phase < r ? (phase / r) : (1.0f - (phase - r) / (1.0f - r));
    v = v * 2.0f - 1.0f;
    v = 0.85f * v + 0.15f * sinf(phase * 2.0f * PI_F * 3.0f);
    return clampf(v, -1.0f, 1.0f);
}

static const char *const demo_songs[] = {
    "Cryin' Through The Rain",
    NULL,                                   /* manual play */
    "Very Long Song Title That Will Not Fit On Screen",
};

void ui_demo_update(ui_data_t *d)
{
    uint32_t t = ui_port_millis();

    demo_rise      = 0.1f + 0.8f * tri(t, 12000);
    d->wave_sample = demo_wave;
    d->rise_pct    = demo_rise * 100.0f;
    d->fall_pct    = 100.0f - d->rise_pct;

    d->adsr_valid = true;
    d->attack_s   = 0.1f + 2.0f * tri(t, 9000);
    d->decay_s    = 0.35f;
    d->sustain    = 0.2f + 0.6f * tri(t, 7000);
    d->release_s  = 1.5f;

    d->lfo_depth  = tri(t, 5000);
    d->lfo_hz     = 0.1f + 20.0f * tri(t, 15000);

    d->volume_valid = true;
    d->volume       = 2.5f * tri(t, 10000);  /* crosses 1.0 -> overdrive */

    d->autoplay_label = demo_songs[(t / 4000) % 3];

    d->wave_center_pwm_valid = true;
    d->wave_center_pwm_pct   = 100.0f * tri(t, 9000);

    /* filter: cycles LPF -> HPF -> off, strength ADSR -> fixed */
    d->filter_valid = true;
    uint32_t ph = (t / 3000) % 3;
    d->lowpass_cutoff_hz     = ph == 0 ? 200.0f + 8000.0f * tri(t, 6000) : 0.0f;
    d->highpass_cutoff_hz    = ph == 1 ? 40.0f + 900.0f * tri(t, 6000) : 0.0f;
    d->filter_adsr_strength  = ph != 2 ? tri(t, 4000) : 0.0f;
    d->filter_fixed_strength = ph == 2 ? 0.6f : 0.0f;
}
