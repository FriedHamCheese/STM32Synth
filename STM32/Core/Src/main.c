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
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "waveform.h"
#include "voice_manager.h"
#include "synth_comms.h"
#include "mux_adc.h"
#include "controls.h"
#include "audio_out.h"
#include "sine_lookup.h"
#include <math.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define V1_TEST_V2
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
WaveformConfig waveform_config =
{
    .rise_pct   = 0.5f,
    .fall_pct   = 0.5f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_SINE,
    .max_output = 350.0f
};

VolumeOscillationParam volume_oscillator_param = {
    .frequency = 1.0f/2.0f,
    .completion = 0.0f,
    .strength = 0.00f
};

VoiceManager voice_manager;
//Master volume can be from 0.0 to 10.0x, overdrive woo
float master_volume = 1.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  mux_adc_init();
  
  #ifdef IS_MASTER
  sine_lookup_init();
  voice_manager_init(&voice_manager, AUDIO_SAMPLE_RATE_HZ, AUDIO_SAMPLE_RATE_HZ);
  audio_out_init(&voice_manager, &waveform_config, &volume_oscillator_param, &master_volume);
  controls_init();
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_Base_Start_IT(&htim4);
  set_audio_output_value(0, 0);
  boot_calibrate();
  #else
  init_slave();
  #endif

  #ifdef V1_TEST_V2
  memset((void*)g_keys, 0, sizeof(g_keys));  
  memset((void*)g_baseline, 0, sizeof(g_baseline));
  uint32_t last_interval_ms = 0;
  uint8_t test_stage = 0;
  #endif
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_poll = 0;
  while (1)
  {
    #ifndef V1_TEST_V2
    controls_update(&waveform_config, &volume_oscillator_param, &master_volume);
    #endif

    uint32_t now = HAL_GetTick();
    if (now - last_poll >= 10)
    {
      last_poll = now;
      #ifdef IS_MASTER
      poll_slaves();
      process_keys(&voice_manager, voice_manager_note_on, voice_manager_note_off, now);
      #else
      update_keyframe();
      #endif
    }
    
    #if defined(V1_TEST_V2) && defined(IS_MASTER)
    const int16_t trigger_keypress = PRESS_THRESHOLD + 1;
    
    if(now - last_interval_ms >= 2000){
      switch (test_stage) {
        case 0:
        	g_keys[3][0] = trigger_keypress;
        	break;
        case 1:
        	g_keys[3][4] = trigger_keypress;
        	break;
        case 2:
        	g_keys[3][7] = trigger_keypress;
        	break;
        case 3:
        	g_keys[5][0] = trigger_keypress;
        	break;
        case 4:
        	g_keys[5][4] = trigger_keypress;
        	break;
        case 5:
        	g_keys[5][7] = trigger_keypress;
        	break;
        case 6:
        	g_keys[6][0] = trigger_keypress;
        	break;
        case 7:
        	g_keys[6][4] = trigger_keypress;
        	break;
        case 8:
        	g_keys[6][7] = trigger_keypress;
        	break;
      }
      
      test_stage++;
      test_stage %= 5;
      last_interval_ms = now;
    }
    #endif
    
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
