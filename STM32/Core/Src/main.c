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
#include "dma.h"
#include "i2c.h"
#include "spi.h"
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
#include "adsr.h"
#include "autoplay.h"
#include "ui.h"
#include "ui_data.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
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
    .rise_pct   = 0.50f,
    .fall_pct   = 0.50f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_SINE,
    .max_output = 350.0f
};

VolumeOscillationParam volume_oscillator_param = {
    .frequency = 1.0f/2.0f,
    .completion = 0.0f,
    .strength = 0.00f
};

Autoplay autoplay = {
  .steps = t2theme,
  .step_started_ms = 0,
  .length = 28,
  .index = 0,
  .playing = false
};
//Set with adsr_set_params() at startup and by controls_update_adsr()
AdsrParam adsr_param;

VoiceManager voice_manager;
//Master volume can be from 0.0 to 10.0x, overdrive woo
float master_volume = 1.0f;

ui_data_t ui_data;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#ifdef IS_MASTER
static void update_test_c_major_chord(uint32_t now_ms)
{
  static uint32_t last_change_ms = 0;
  static uint8_t stage = 0;
  static const uint8_t key_ids[] = {
    C3, E3, G3,
    C4, E4, G4,
    C5, E5, G5
  };
  const uint8_t key_count = sizeof(key_ids) / sizeof(key_ids[0]);

  if (now_ms - last_change_ms < 2000)
    return;

  last_change_ms = now_ms;

  if (stage < key_count) {
    const uint8_t key_id = key_ids[stage];
    const uint8_t slave = key_id / 12;
    const uint8_t key = key_id % 12;
    g_keys[slave][key] = g_baseline[slave][key] + PRESS_THRESHOLD + 1;
    stage++;
  } else {
    for (uint8_t i = 0; i < key_count; ++i) {
      const uint8_t slave = key_ids[i] / 12;
      const uint8_t key = key_ids[i] % 12;
      g_keys[slave][key] = g_baseline[slave][key];
    }
    stage = 0;
  }
}
#endif

static void handle_autoplay_button_pressed(uint32_t now_ms){
  static GPIO_PinState autoplay_button_last = GPIO_PIN_SET;
  static uint8_t autoplay_song_index = 0;
  const GPIO_PinState autoplay_button_status = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);
  const uint8_t autoplay_button_pressed = (
      autoplay_button_last == GPIO_PIN_SET 
      && autoplay_button_status == GPIO_PIN_RESET
  );
  
  if (autoplay_button_pressed) {
    voice_manager_all_off(&voice_manager);
    autoplay_song_index++;
    const uint8_t stop_autoplay = autoplay_song_index > autoplay_song_count;
    if(stop_autoplay){
      autoplay_stop(&autoplay, &voice_manager);
      autoplay_song_index = 0;
    }else{
      autoplay.steps = autoplay_songs[autoplay_song_index-1].steps;
      autoplay.length = autoplay_songs[autoplay_song_index-1].length;
      autoplay_start(&autoplay, &voice_manager, now_ms);
    }
  }
  autoplay_button_last = autoplay_button_status;
  autoplay_update(&autoplay, &voice_manager, now_ms); 
}

static float ui_adsr_rate_to_seconds(float rate)
{
  if (rate >= 1.0f || rate <= 0.0f)
    return 0.0f;
  return 1.0f / (rate * AUDIO_SAMPLE_RATE_HZ);
}

static float ui_wave_sample(float phase)
{
  float sample = (float)waveform_get_point(phase, &waveform_config);
  if (waveform_config.max_output <= 0.0f)
    return 0.0f;
  return (sample / waveform_config.max_output) * 2.0f - 1.0f;
}

static void update_ui_data(void)
{
  ui_data.wave_sample = ui_wave_sample;
  ui_data.rise_pct = waveform_config.rise_pct * 100.0f;
  ui_data.fall_pct = waveform_config.fall_pct * 100.0f;
  ui_data.adsr_valid = true;
  ui_data.attack_s = ui_adsr_rate_to_seconds(adsr_param.attack_rate);
  ui_data.decay_s = ui_adsr_rate_to_seconds(adsr_param.decay_rate);
  ui_data.sustain = adsr_param.sustain_level;
  ui_data.release_s = ui_adsr_rate_to_seconds(adsr_param.release_rate);
  ui_data.lfo_depth = volume_oscillator_param.strength;
  ui_data.lfo_hz = volume_oscillator_param.frequency;
  ui_data.volume_valid = true;
  ui_data.volume = master_volume;
}

void periodic_loop(void)
{
  static uint32_t last_poll = 0;
  uint32_t now = HAL_GetTick();

  controls_update(&waveform_config, &volume_oscillator_param, &master_volume);
  controls_update_adsr(&adsr_param);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12));
  if (now - last_poll >= 10)
  {
    last_poll = now;
    #ifdef IS_MASTER
    poll_slaves();
    #ifdef V1_TEST_V2
    update_test_c_major_chord(now);
    #endif
    if(!autoplay.playing)
    	process_keys(&voice_manager, voice_manager_note_on, voice_manager_note_off, now);
    #else
    update_keyframe();
    #endif
  }
  
  #ifdef IS_MASTER
  handle_autoplay_button_pressed(now);
  #endif
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
  MX_DMA_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  mux_adc_init();
  
  #ifdef IS_MASTER
  sine_lookup_init();
  adsr_set_params(&adsr_param, 0, 0, 1.0f, 0, AUDIO_SAMPLE_RATE_HZ);
  voice_manager_init(&voice_manager, AUDIO_SAMPLE_RATE_HZ, AUDIO_SAMPLE_RATE_HZ, &adsr_param);
  audio_out_init(&voice_manager, &waveform_config, &volume_oscillator_param, &master_volume);
  controls_init();
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_TIM_Base_Start_IT(&htim4);
  set_audio_output_value(0, 0);
  boot_calibrate();
  update_ui_data();
  ui_init(&ui_data);
  #else
  init_slave();
  #endif

  #ifdef V1_TEST_V2
  memset((void*)g_keys, 0, sizeof(g_keys));  
  memset((void*)g_baseline, 0, sizeof(g_baseline));
  #endif
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    #ifdef IS_MASTER
    update_ui_data();
    ui_task(&ui_data);
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
