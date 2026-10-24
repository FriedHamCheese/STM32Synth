#include "controls.h"
#include "mux_adc.h"
#include "volume_oscillation.h"
#include "audio_out.h"
#include "audparam_ranges.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#include <math.h>

#define CONTROL_SMOOTHING   0.125f
#define BUTTON_DEBOUNCE_MS  25U

/* Waveform, volume oscillator, and master volume. */
#define RISE_MUX_CH         0
#define FALL_MUX_CH         1
#define WAVE_MIDPOINT_CH    6
#define METRONOME_BPM_CH    7
#define VOSC_FREQUENCY_CH   9
#define VOSC_STRENGTH_CH    10
#define MASTER_VOLUME_CH    11

/* ADSR parameters. */
#define ADSR_ATTACK_MUX_CH   2
#define ADSR_DECAY_MUX_CH    3
#define ADSR_SUSTAIN_MUX_CH  4
#define ADSR_RELEASE_MUX_CH  5

typedef struct {
    GPIO_PinState raw_state;
    GPIO_PinState stable_state;
    uint32_t      changed_at;
} DebouncedButton;

static DebouncedButton s_btn1 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static DebouncedButton s_btn2 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };

static float s_smoothed_rise  = 0.5f;
static float s_smoothed_fall  = 0.5f;
static float s_wave_midpoint  = 0.5f;
static float s_smoothed_bpm   = 0.5f;
static float s_smoothed_vosc_frequency = 0.5f;
static float s_smoothed_vosc_strength = 0.5f;
static float s_smoothed_master_volume = 0.5f;

static float s_smoothed_attack  = 0.0f;
static float s_smoothed_decay   = 0.0f;
static float s_smoothed_sustain = 0.0f;
static float s_smoothed_release = 0.0f;

static float adc_value_to_ratio(uint16_t adc_value);
static float ratio_to_master_volume(float control);
static float smooth_control(float new_value, float old_value);
static void update_one_button(DebouncedButton *btn, GPIO_PinState raw, WaveShape *shape);
static float read_smoothed_pot(uint8_t mux_ch, float *smoothed);
static uint32_t pot_to_ms(float pot, float max_ms);

void controls_init(void) { /* mux and GPIO already inited by CubeMX */ }

void controls_update(WaveformConfig *cfg, VolumeOscillationParam *vosc, float *master_volume, MetronomeParam *metro)
{
    s_smoothed_rise = smooth_control(adc_value_to_ratio(mux_read(RISE_MUX_CH)), s_smoothed_rise);
    s_smoothed_fall = smooth_control(adc_value_to_ratio(mux_read(FALL_MUX_CH)), s_smoothed_fall);
    cfg->rise_pct = s_smoothed_rise;
    cfg->fall_pct = s_smoothed_fall;
    
    s_smoothed_vosc_frequency = smooth_control(
        adc_value_to_ratio(mux_read(VOSC_FREQUENCY_CH)), s_smoothed_vosc_frequency
    );
    s_smoothed_vosc_strength = smooth_control(
        adc_value_to_ratio(mux_read(VOSC_STRENGTH_CH)), s_smoothed_vosc_strength
    );
    s_smoothed_master_volume = smooth_control(
        adc_value_to_ratio(mux_read(MASTER_VOLUME_CH)), s_smoothed_master_volume
    );
    s_wave_midpoint = smooth_control(
        adc_value_to_ratio(mux_read(WAVE_MIDPOINT_CH)), s_wave_midpoint
    );
    if (s_wave_midpoint < 0.001f)
        s_wave_midpoint = 0.001f;
    if (s_wave_midpoint > 0.999f)
        s_wave_midpoint = 0.999f;
    cfg->midpoint = s_wave_midpoint;

    s_smoothed_bpm = smooth_control(
        adc_value_to_ratio(mux_read(METRONOME_BPM_CH)), s_smoothed_bpm
    );
    metronome_set_bpm(
        metro,
        METRONOME_MIN_BPM
        + s_smoothed_bpm * (METRONOME_MAX_BPM - METRONOME_MIN_BPM)
    );
    /* SW6 is the former ADSR mode switch; active-low input with pull-up. */
    metronome_set_enabled(
        metro,
        HAL_GPIO_ReadPin(SW6_GPIO_Port, SW6_Pin) == GPIO_PIN_RESET
    );

    vosc->frequency = (
        VOL_OSC_MIN_HZ
        + s_smoothed_vosc_frequency * (VOL_OSC_MAX_HZ - VOL_OSC_MIN_HZ)
    );
    vosc->strength = s_smoothed_vosc_strength;
    *master_volume = ratio_to_master_volume(s_smoothed_master_volume);

    update_one_button(&s_btn1, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12), &cfg->rise_shape);
    update_one_button(&s_btn2, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11), &cfg->fall_shape);
}

void controls_update_adsr(AdsrParam *adsr)
{
    adsr_set_params(
        adsr,
        pot_to_ms(
            read_smoothed_pot(ADSR_ATTACK_MUX_CH, &s_smoothed_attack),
            ADSR_ATTACK_MAX_MS
        ),
        pot_to_ms(
            read_smoothed_pot(ADSR_DECAY_MUX_CH, &s_smoothed_decay),
            ADSR_DECAY_MAX_MS
        ),
        read_smoothed_pot(ADSR_SUSTAIN_MUX_CH, &s_smoothed_sustain),
        pot_to_ms(
            read_smoothed_pot(ADSR_RELEASE_MUX_CH, &s_smoothed_release),
            ADSR_RELEASE_MAX_MS
        ),
        AUDIO_SAMPLE_RATE_HZ
    );
}

static float adc_value_to_ratio(uint16_t adc_value)
{
  /*
  This is a function that scales ADC readings to 10-3990 scale so that potentiometer full off/on seems linear.
  */
  const float ADC_CONTROL_MINF = 10.0f;
  const float ADC_CONTROL_MAXF = 3990.0f;
  const float ADC_CONTROL_RANGE_RECIP = 1.0f / (ADC_CONTROL_MAXF - ADC_CONTROL_MINF);
  
  float raw = (float)adc_value;

  if (raw < ADC_CONTROL_MINF) raw = ADC_CONTROL_MINF;
  if (raw > ADC_CONTROL_MAXF) raw = ADC_CONTROL_MAXF;

  /* it is max-raw because left-most pot rotation is actually increasing the voltage/ADC*/
  return (ADC_CONTROL_MAXF - raw) * ADC_CONTROL_RANGE_RECIP;
}

static float ratio_to_master_volume(float control)
{
    const float volume_cutoff_control = 0.05f;
    const float volume_midpoint = 0.5f;
    const float volume_linear_range = volume_midpoint - volume_cutoff_control;
    const float volume_override_range = 1.0f - volume_midpoint;

    if (control <= volume_cutoff_control)
        return MASTER_VOLUME_MIN;

    if (control <= volume_midpoint)
        return ((control - volume_cutoff_control) / volume_linear_range)
             * MASTER_VOLUME_MID;

    return MASTER_VOLUME_MID
         + (
             ((control - volume_midpoint) / volume_override_range)
             * (MASTER_VOLUME_MAX - MASTER_VOLUME_MID)
         );
}

static float smooth_control(float new_value, float old_value)
{
    return old_value + CONTROL_SMOOTHING * (new_value - old_value);
}

static void update_one_button(DebouncedButton *btn, GPIO_PinState raw, WaveShape *shape)
{
    uint32_t now = HAL_GetTick();
    if (raw != btn->raw_state) { btn->raw_state = raw; btn->changed_at = now; }
    if ((raw != btn->stable_state) && ((now - btn->changed_at) >= BUTTON_DEBOUNCE_MS)) {
        btn->stable_state = raw;
        if (btn->stable_state == GPIO_PIN_RESET)
            *shape = (WaveShape)((*shape + 1U) % 3U);
    }
}

static float read_smoothed_pot(uint8_t mux_ch, float *smoothed)
{
    *smoothed = smooth_control(adc_value_to_ratio(mux_read(mux_ch)), *smoothed);
    return *smoothed;
}

/* Exponential so each part of the pot's turn feels equally useful:
   with 1-2000ms, the middle of the pot is ~45ms rather than ~1000ms. */
static uint32_t pot_to_ms(float pot, float max_ms)
{
    return (uint32_t)(ADSR_MIN_MS * powf(max_ms / ADSR_MIN_MS, pot) + 0.5f);
}
