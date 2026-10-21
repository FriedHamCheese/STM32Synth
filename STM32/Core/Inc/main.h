/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void main_runtime_iter();
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUILTIN_LED_Pin GPIO_PIN_13
#define BUILTIN_LED_GPIO_Port GPIOC
#define MUX_S0_Pin GPIO_PIN_0
#define MUX_S0_GPIO_Port GPIOA
#define MUX_S1_Pin GPIO_PIN_1
#define MUX_S1_GPIO_Port GPIOA
#define MUX_S2_Pin GPIO_PIN_2
#define MUX_S2_GPIO_Port GPIOA
#define MUX_S3_Pin GPIO_PIN_3
#define MUX_S3_GPIO_Port GPIOA
#define MUX_SIG_Pin GPIO_PIN_4
#define MUX_SIG_GPIO_Port GPIOA
#define DIP_4_Pin GPIO_PIN_0
#define DIP_4_GPIO_Port GPIOB
#define DIP_2_Pin GPIO_PIN_1
#define DIP_2_GPIO_Port GPIOB
#define DIP_1_Pin GPIO_PIN_10
#define DIP_1_GPIO_Port GPIOB
#define LCD_CS_Pin GPIO_PIN_8
#define LCD_CS_GPIO_Port GPIOA
#define LCD_RST_Pin GPIO_PIN_9
#define LCD_RST_GPIO_Port GPIOA
#define LCD_DC_Pin GPIO_PIN_10
#define LCD_DC_GPIO_Port GPIOA
#define SW2_Pin GPIO_PIN_11
#define SW2_GPIO_Port GPIOA
#define SW1_Pin GPIO_PIN_12
#define SW1_GPIO_Port GPIOA
#define AUDIO_RIGHT_PWM_Pin GPIO_PIN_15
#define AUDIO_RIGHT_PWM_GPIO_Port GPIOA
#define AUDIO_LEFT_PWM_Pin GPIO_PIN_3
#define AUDIO_LEFT_PWM_GPIO_Port GPIOB
#define SW3_Pin GPIO_PIN_4
#define SW3_GPIO_Port GPIOB
#define SW4_Pin GPIO_PIN_5
#define SW4_GPIO_Port GPIOB
#define SW5_Pin GPIO_PIN_8
#define SW5_GPIO_Port GPIOB
#define SW6_Pin GPIO_PIN_9
#define SW6_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
