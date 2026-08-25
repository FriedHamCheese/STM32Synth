// ============================================================
// touch_sensor.h — capacitive touch driver (HAL GPIO RC method)
// ============================================================
// Generic sensing module. The caller passes a list of GPIO pins
// to sense. Uses the RC charge-timing technique — no TSC peripheral
// needed (the STM32F401 doesn't have TSC).
//
//   const TouchPin key_pins[] = {
//       { GPIOB, GPIO_PIN_6 },   // key 1 (B6)
//       { GPIOB, GPIO_PIN_8 },   // key 2 (B8)
//   };
//
//   Touch_Init(key_pins, 2);     // once, no fingers touching
//   uint16_t mask = Touch_Scan(); // bit N = key N pressed
//
// Hardware per pad: 3.3V ── 2.2 MΩ ──┬── GPIO ── insulated pad
// bit 0 = key_pins[0], bit 1 = key_pins[1], ...
// ============================================================

#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define TOUCH_MAX_KEYS  18

// One sensor = a GPIO port + pin
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} TouchPin;

// Initialize. Pass the pin list + how many. Call once, fingers away.
void Touch_Init(const TouchPin *pins, uint8_t num_keys);

// Scan all keys. Returns bitmask: bit N set = pins[N] pressed.
uint16_t Touch_Scan(void);

// Convenience: true if key index `key` is pressed.
bool Touch_IsPressed(uint8_t key);

#endif // TOUCH_SENSOR_H
