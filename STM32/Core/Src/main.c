/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "waveform.h"
#include "voice_manager.h"
#include <math.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
///Highest valid value is 350 for full speaker push, lowest is 0 for full speaker pull.
///175 is center speaker position.

#define AUDIO_SAMPLE_RATE_HZ 48000.0f
#define ADC_MAX_VALUE        4095.0f
#define CONTROL_SMOOTHING    0.15f
#define BUTTON_DEBOUNCE_MS   25U

WaveformConfig waveform_config =
{
    .rise_pct = 0.5f,
    .fall_pct = 0.5f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_TRIANGLE,
    .max_output = 350.0f
};

Waveform wave_1 = { .waveform_completion_ratio = 0.0f,
                    .waveform_completion_increment = 500.0f / AUDIO_SAMPLE_RATE_HZ };
Waveform wave_2 = { .waveform_completion_ratio = 0.0f,
                    .waveform_completion_increment = 200.0f / AUDIO_SAMPLE_RATE_HZ };

typedef struct
{
    GPIO_PinState raw_state;
    GPIO_PinState stable_state;
    uint32_t changed_at;
} DebouncedButton;

static DebouncedButton button1 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static DebouncedButton button2 = { GPIO_PIN_SET, GPIO_PIN_SET, 0U };
static float smoothed_rise = 0.5f;
static float smoothed_fall = 0.5f;

VoiceManager voice_manager;

/* USER CODE END PV */

void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
void set_audio_output_value(wavegen_output_t left_value, wavegen_output_t right_value){
	TIM3->CCR1 = right_value;
	TIM3->CCR2 = left_value;
}

static void update_one_button(DebouncedButton *button, GPIO_PinState raw,
                              WaveShape *shape)
{
    uint32_t now = HAL_GetTick();

    if (raw != button->raw_state)
    {
        button->raw_state = raw;
        button->changed_at = now;
    }

    if ((raw != button->stable_state) &&
        ((now - button->changed_at) >= BUTTON_DEBOUNCE_MS))
    {
        button->stable_state = raw;
        if (button->stable_state == GPIO_PIN_RESET)
        {
            *shape = (WaveShape)((*shape + 1U) % 3U);
        }
    }
}

static uint32_t read_adc_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
        return 0;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return 0;

    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }

    uint32_t value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}

void update_potentiometers(void)
{
    uint32_t rise_adc = read_adc_channel(ADC_CHANNEL_0);
    uint32_t fall_adc = read_adc_channel(ADC_CHANNEL_1);

    smoothed_rise += CONTROL_SMOOTHING *
                     (((float)rise_adc / ADC_MAX_VALUE) - smoothed_rise);

    smoothed_fall += CONTROL_SMOOTHING *
                     (((float)fall_adc / ADC_MAX_VALUE) - smoothed_fall);

    waveform_config.rise_pct = smoothed_rise;
    waveform_config.fall_pct = smoothed_fall;
}

void update_buttons(void)
{
    update_one_button(&button1, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0),
                      &waveform_config.rise_shape);
    update_one_button(&button2, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1),
                      &waveform_config.fall_shape);
}

void next_audio_sample(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM4)
        return;

    wave_1.waveform_completion_ratio += wave_1.waveform_completion_increment;
    wave_2.waveform_completion_ratio += wave_2.waveform_completion_increment;

    if (wave_1.waveform_completion_ratio >= 1.0f)
        wave_1.waveform_completion_ratio -= 1.0f;
    if (wave_2.waveform_completion_ratio >= 1.0f)
        wave_2.waveform_completion_ratio -= 1.0f;

    set_audio_output_value(
        waveform_get_point(wave_2.waveform_completion_ratio, &waveform_config),
        waveform_get_point(wave_1.waveform_completion_ratio, &waveform_config));
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    next_audio_sample(htim);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* USER CODE BEGIN 2 */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  waveform_init();
  // 2000f can be change naja, voice manager not fully integrate.
  voice_manager_init(&voice_manager, AUDIO_SAMPLE_RATE_HZ, 2000.0f);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_Base_Start_IT(&htim4);
  set_audio_output_value(
      (wavegen_output_t)(waveform_config.max_output / 2.0f),
      (wavegen_output_t)(waveform_config.max_output / 2.0f)
  );
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  update_potentiometers();
	  update_buttons();

	  HAL_Delay(10);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
