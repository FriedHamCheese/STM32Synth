#include "mux_adc.h"

void mux_adc_init(void)
{
    ADC_ChannelConfTypeDef s = {0};
    s.Channel      = MUX_ADC_CH;
    s.Rank         = 1;
    s.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &s);
}

uint16_t mux_read(uint8_t ch)
{
    HAL_GPIO_WritePin(MUX_PORT, MUX_S0_PIN, (ch & 0x1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_PORT, MUX_S1_PIN, (ch & 0x2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_PORT, MUX_S2_PIN, (ch & 0x4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_PORT, MUX_S3_PIN, (ch & 0x8) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Settle ~2µs at 84MHz */
    for (volatile uint32_t i = 0; i < 170; i++);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 5);
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}
