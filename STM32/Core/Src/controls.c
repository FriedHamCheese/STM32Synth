#include "controls.h"
#include "mux_adc.h"
#include "volume_oscillation.h"
#include "audio_out.h"
#include "stm32f4xx_hal.h"

#include <math.h>

#define CONTROL_SMOOTHING   0.50f
#define BUTTON_DEBOUNCE_MS  25U
#define ADC_MAX_VALUE       4095.0f

/* PB9 low selects the ADSR mux mode. */
#define CONTROLS_MODE_PIN GPIO_PIN_9
#define CONTROLS_MODE_PORT GPIOB

/* Mode 1: waveform, volume oscillator, and master volume. */
#define RISE_MUX_CH          1
#define FALL_MUX_CH          2
#define VOSC_FREQUENCY_CH   3
#define VOSC_STRENGTH_CH    4
#define MASTER_VOLUME_CH    5

/* Mode 2: ADSR parameters. */
#define ADSR_ATTACK_MUX_CH   1
#define ADSR_DECAY_MUX_CH    2
#define ADSR_SUSTAIN_MUX_CH  3
#define ADSR_RELEASE_MUX_CH  4

/* Time ranges for a full 0<->1 swing, pot fully left -> fully right */
#define ADSR_MIN_MS          1.0f
#define ADSR_ATTACK_MAX_MS   2000.0f
#define ADSR_DECAY_MAX_MS    2000.0f
#define ADSR_RELEASE_MAX_MS  4000.0f

typedef struct {
    GPIO_PinState raw_state;
    GPIO_PinState stable_state;
    uint32_t      changed_at;
} DebouncedButton;

static DebouncedButton s_btn1 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static DebouncedButton s_btn2 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static float s_smoothed_rise  = 0.5f;
static float s_smoothed_fall  = 0.5f;
static float s_smoothed_attack  = 0.0f;
static float s_smoothed_decay   = 0.0f;
static float s_smoothed_sustain = 0.0f;
static float s_smoothed_release = 0.0f;
static uint8_t s_adsr_mode = 0;
static GPIO_PinState s_last_mode_pin = GPIO_PIN_SET;

static float read_pot_normalized(uint8_t mux_ch)
{
    return 1.0f - (mux_read(mux_ch) * (1.0/ADC_MAX_VALUE));
}

void controls_init(void) { /* mux and GPIO already inited by CubeMX */ }

uint8_t controls_is_adsr_mode(void)
{
    return s_adsr_mode;
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

void controls_update(WaveformConfig *cfg, VolumeOscillationParam *vosc, float *master_volume)
{
    const GPIO_PinState mode_pin = HAL_GPIO_ReadPin(CONTROLS_MODE_PORT, CONTROLS_MODE_PIN);
    if (s_last_mode_pin == GPIO_PIN_SET && mode_pin == GPIO_PIN_RESET)
        s_adsr_mode = !s_adsr_mode;
    s_last_mode_pin = mode_pin;

    if (s_adsr_mode)
        return;

    s_smoothed_rise += CONTROL_SMOOTHING * (read_pot_normalized(RISE_MUX_CH) - s_smoothed_rise);
    s_smoothed_fall += CONTROL_SMOOTHING * (read_pot_normalized(FALL_MUX_CH) - s_smoothed_fall);
    cfg->rise_pct = s_smoothed_rise;
    cfg->fall_pct = s_smoothed_fall;
    
    const float volume_osc_min_hz = 0.1f;
    const float volume_osc_max_hz = 20.0f;
    const float master_volume_max_value = 10.0f;
    vosc->frequency = volume_osc_min_hz
                    + (read_pot_normalized(VOSC_FREQUENCY_CH)
                      * (volume_osc_max_hz - volume_osc_min_hz)
                      );
    vosc->strength = read_pot_normalized(VOSC_STRENGTH_CH);
    *master_volume = read_pot_normalized(MASTER_VOLUME_CH) * master_volume_max_value;

    update_one_button(&s_btn1, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12), &cfg->rise_shape);
    update_one_button(&s_btn2, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11), &cfg->fall_shape);
}

static float read_smoothed_pot(uint8_t mux_ch, float *smoothed)
{
    *smoothed += CONTROL_SMOOTHING * (read_pot_normalized(mux_ch) - *smoothed);
    return *smoothed;
}

/* Exponential so each part of the pot's turn feels equally useful:
   with 1-2000ms, the middle of the pot is ~45ms rather than ~1000ms. */
static uint32_t pot_to_ms(float pot, float max_ms)
{
    return (uint32_t)(ADSR_MIN_MS * powf(max_ms / ADSR_MIN_MS, pot) + 0.5f);
}

void controls_update_adsr(AdsrParam *adsr)
{
    if (!s_adsr_mode)
        return;

    adsr_set_params(
        adsr,
        pot_to_ms(read_smoothed_pot(ADSR_ATTACK_MUX_CH, &s_smoothed_attack), ADSR_ATTACK_MAX_MS),
        pot_to_ms(read_smoothed_pot(ADSR_DECAY_MUX_CH, &s_smoothed_decay), ADSR_DECAY_MAX_MS),
        read_smoothed_pot(ADSR_SUSTAIN_MUX_CH, &s_smoothed_sustain),
        pot_to_ms(read_smoothed_pot(ADSR_RELEASE_MUX_CH, &s_smoothed_release), ADSR_RELEASE_MAX_MS),
        AUDIO_SAMPLE_RATE_HZ
    );
}
