#ifndef __MUX_ADC_H
#define __MUX_ADC_H

#include "stm32f4xx_hal.h"
#include "adc.h"
#include <stdint.h>

/* Pin config — change here for different board layouts */
#define MUX_PORT    GPIOA
#define MUX_S0_PIN  GPIO_PIN_0
#define MUX_S1_PIN  GPIO_PIN_1
#define MUX_S2_PIN  GPIO_PIN_2
#define MUX_S3_PIN  GPIO_PIN_3
#define MUX_ADC_CH  ADC_CHANNEL_4

/* Configure ADC channel once at startup (call before mux_read). */
void     mux_adc_init(void);
/* Read mux channel ch (0–15). Returns 12-bit ADC value (0–4095). */
uint16_t mux_read(uint8_t ch);

#endif /* __MUX_ADC_H */
