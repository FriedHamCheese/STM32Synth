#include "controls.h"
#include "mux_adc.h"
#include "volume_oscillation.h"
#include "stm32f4xx_hal.h"

#define CONTROL_SMOOTHING   0.15f
#define BUTTON_DEBOUNCE_MS  25U
#define ADC_MAX_VALUE       4095.0f

typedef struct {
    GPIO_PinState raw_state;
    GPIO_PinState stable_state;
    uint32_t      changed_at;
} DebouncedButton;

static DebouncedButton s_btn1 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static DebouncedButton s_btn2 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static float s_smoothed_rise  = 0.5f;
static float s_smoothed_fall  = 0.5f;

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

void controls_update(WaveformConfig *cfg, VolumeOscillationParam *vosc, float *master_volume)
{
    /* Pots via mux — ch0=rise period, ch1=fall period, ch6=volume oscillation frequency,
       ch7=volume oscillation strength, ch2-5/ch8-11 stubbed (ADSR/LFO/pitch) */
    s_smoothed_rise += CONTROL_SMOOTHING * ((mux_read(0) / ADC_MAX_VALUE) - s_smoothed_rise);
    s_smoothed_fall += CONTROL_SMOOTHING * ((mux_read(1) / ADC_MAX_VALUE) - s_smoothed_fall);
    cfg->rise_pct = s_smoothed_rise;
    cfg->fall_pct = s_smoothed_fall;
    
    const float volume_osc_min_hz = 0.1f;
    const float volume_osc_max_hz = 20.0f;
    const float master_volume_max_value = 10.0f;
    vosc->frequency = volume_osc_min_hz 
                    + ((mux_read(6) / ADC_MAX_VALUE) 
                      * (volume_osc_max_hz - volume_osc_min_hz)
                      );
    vosc->strength = mux_read(7) / ADC_MAX_VALUE;
    *master_volume = (mux_read(8) / ADC_MAX_VALUE) * master_volume_max_value;

    /* ponytail: ch2-11 (ADSR/LFO/pitch) stubbed until Putt/Mind add those fields to WaveformConfig */

    update_one_button(&s_btn1, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0), &cfg->rise_shape);
    update_one_button(&s_btn2, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1), &cfg->fall_shape);
}
