#ifndef __MUX_ADC_H
#define __MUX_ADC_H

/* ADC1 + 74HC4067 mux (slave boards).
   Pins: S0=PA0, S1=PA1, S2=PA2, S3=PA3, Z=PA4 (ADC1_IN4).
   hadc1 owned by CubeMX adc.c — include adc.h to access it. */

#include "adc.h"
#include <stdint.h>

/* Read mux channel ch (0–11) via ADC1. Returns 12-bit unsigned result. */
uint16_t mux_read(uint8_t ch);

#endif /* __MUX_ADC_H */
