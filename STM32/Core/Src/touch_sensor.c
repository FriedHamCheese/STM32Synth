// ============================================================
// touch_sensor.c — capacitive touch driver (HAL GPIO RC method)
// ============================================================
// Internal implementation. Uses the RC charge-timing technique,
// fully HAL-based (no TSC peripheral — the STM32F401 has none).
//
// Timing is measured with a HARDWARE TIMER (TIM2), NOT by counting
// CPU loop iterations. This makes the measurement immune to
// interrupts (SysTick, etc.), which would otherwise corrupt the
// timing and add random jitter to every reading.
//
// For each pad:
//   - discharge the pad by driving the GPIO LOW
//   - switch to input (no pull), let the external 2.2 MΩ pull it up
//   - start the hardware timer, wait until the pin reaches HIGH
//   - read the timer → charge time
//   - a finger near the pad adds capacitance → charge takes longer
// ============================================================

#include "touch_sensor.h"

// ── Tuning constants ────────────────────────────────────────────────────
#define ACQ_COUNT        16     // samples averaged per read
#define CAL_SAMPLES      16     // samples averaged during calibration
#define TOUCH_THRESHOLD  1.5f   // pressed = reading > 1.5× baseline
#define CHARGE_TIMEOUT   100000 // safety timeout, in timer ticks

// ── Hardware timer for interrupt-safe timing ────────────────────────────
static TIM_HandleTypeDef htim2;

static void timer_init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();

    // TIM2 is a 32-bit free-running counter. Its exact tick rate does not
    // matter here, because the RC method compares a reading against a
    // baseline measured on the same timer — the ratio is clock-independent.
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFF;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start(&htim2);
}

// ── Module state ────────────────────────────────────────────────────────
static const TouchPin *key_pins = 0;
static uint8_t          num_keys = 0;
static uint32_t         baseline[TOUCH_MAX_KEYS];

// ── Single RC charge-timing read on one pin ─────────────────────────────
static uint32_t read_pin(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef g = {0};

    // 1. Discharge: drive LOW
    g.Pin   = pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &g);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    for (volatile int i = 0; i < 2000; i++) { }   // small settle

    // 2. Input, no pull — external 2.2 MΩ charges it up
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &g);

    // 3. Measure charge time with the hardware timer (interrupt-safe)
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) {
        if (__HAL_TIM_GET_COUNTER(&htim2) > CHARGE_TIMEOUT) {
            break;
        }
    }
    return __HAL_TIM_GET_COUNTER(&htim2);
}

// ── Average several reads ───────────────────────────────────────────────
static uint32_t read_avg(const TouchPin *p) {
    uint32_t sum = 0;
    for (int j = 0; j < ACQ_COUNT; j++) {
        sum += read_pin(p->port, p->pin);
    }
    return sum / ACQ_COUNT;
}

// ── Initialize + calibrate ──────────────────────────────────────────────
void Touch_Init(const TouchPin *pins, uint8_t n) {
    key_pins = pins;
    num_keys = (n > TOUCH_MAX_KEYS) ? TOUCH_MAX_KEYS : n;

    timer_init();

    for (int i = 0; i < num_keys; i++) {
        uint32_t sum = 0;
        for (int j = 0; j < CAL_SAMPLES; j++) {
            sum += read_pin(key_pins[i].port, key_pins[i].pin);
        }
        baseline[i] = sum / CAL_SAMPLES;
    }
}

// ── Scan all keys, return bitmask ───────────────────────────────────────
uint16_t Touch_Scan(void) {
    uint16_t pressed = 0;

    for (int i = 0; i < num_keys; i++) {
        uint32_t avg = read_avg(&key_pins[i]);

        // Finger adds capacitance → charge takes LONGER → count RISES.
        // Pressed = reading above 1.5× baseline.
        if (avg > (uint32_t)(baseline[i] * TOUCH_THRESHOLD)) {
            pressed |= (1u << i);
        }
    }

    return pressed;
}

// ── Convenience: single-key check ───────────────────────────────────────
bool Touch_IsPressed(uint8_t key) {
    return (Touch_Scan() & (1u << key)) != 0;
}
