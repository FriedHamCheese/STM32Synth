#include "controls.h"
#include "mux_adc.h"
#include "stm32f4xx_hal.h"

#include <math.h>

#define CONTROL_SMOOTHING   0.15f
#define BUTTON_DEBOUNCE_MS  25U
#define ADC_MAX_VALUE       4095.0f

/* ADSR pots — mux channels, check against PCB */
#define ADSR_ATTACK_MUX_CH   2
#define ADSR_DECAY_MUX_CH    3
#define ADSR_SUSTAIN_MUX_CH  4
#define ADSR_RELEASE_MUX_CH  5

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

void controls_init(void) { /* mux and GPIO already inited by CubeMX */ }

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

void controls_update(WaveformConfig *cfg)
{
    /* Pots via mux — ch0=rise period, ch1=fall period, ch2-11 stubbed (ADSR/LFO/pitch) */
    s_smoothed_rise += CONTROL_SMOOTHING * ((mux_read(0) / ADC_MAX_VALUE) - s_smoothed_rise);
    s_smoothed_fall += CONTROL_SMOOTHING * ((mux_read(1) / ADC_MAX_VALUE) - s_smoothed_fall);
    cfg->rise_pct = s_smoothed_rise;
    cfg->fall_pct = s_smoothed_fall;
    /* ponytail: ch2-11 (ADSR/LFO/pitch) stubbed until Putt/Mind add those fields to WaveformConfig */

    update_one_button(&s_btn1, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0), &cfg->rise_shape);
    update_one_button(&s_btn2, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1), &cfg->fall_shape);
}

static float read_smoothed_pot(uint8_t mux_ch, float *smoothed)
{
    *smoothed += CONTROL_SMOOTHING * ((mux_read(mux_ch) / ADC_MAX_VALUE) - *smoothed);
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
    adsr->attack_ms     = pot_to_ms(read_smoothed_pot(ADSR_ATTACK_MUX_CH, &s_smoothed_attack), ADSR_ATTACK_MAX_MS);
    adsr->decay_ms      = pot_to_ms(read_smoothed_pot(ADSR_DECAY_MUX_CH, &s_smoothed_decay), ADSR_DECAY_MAX_MS);
    adsr->sustain_level = read_smoothed_pot(ADSR_SUSTAIN_MUX_CH, &s_smoothed_sustain);
    adsr->release_ms    = pot_to_ms(read_smoothed_pot(ADSR_RELEASE_MUX_CH, &s_smoothed_release), ADSR_RELEASE_MAX_MS);
}
