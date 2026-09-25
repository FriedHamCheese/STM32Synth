#include "mux_adc.h"

uint16_t mux_read(uint8_t ch)
{
    /* Drive S0-S3 from PA0-PA3 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, (ch & 0x1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, (ch & 0x2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (ch & 0x4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, (ch & 0x8) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Settle: ~2µs at 84MHz */
    for (volatile uint32_t i = 0; i < 170; i++);

    /* Single conversion on IN4 */
    ADC_ChannelConfTypeDef s = {0};
    s.Channel      = ADC_CHANNEL_4;
    s.Rank         = 1;
    s.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &s);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 5);
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}
