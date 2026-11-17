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
#include "stm32g4xx_hal.h"

#include "stm32g4xx_nucleo.h"
#include <stdio.h>

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN_OK_Pin GPIO_PIN_0
#define BTN_OK_GPIO_Port GPIOF
#define BTN_OK_EXTI_IRQn EXTI0_IRQn
#define TFT_BLK_Pin GPIO_PIN_0
#define TFT_BLK_GPIO_Port GPIOA
#define BTN_UP_Pin GPIO_PIN_1
#define BTN_UP_GPIO_Port GPIOA
#define BTN_UP_EXTI_IRQn EXTI1_IRQn
#define SD_CS_Pin GPIO_PIN_4
#define SD_CS_GPIO_Port GPIOA
#define TFT_CS_Pin GPIO_PIN_0
#define TFT_CS_GPIO_Port GPIOB
#define ESP_EN_Pin GPIO_PIN_8
#define ESP_EN_GPIO_Port GPIOA
#define BTN_LED_EN_Pin GPIO_PIN_11
#define BTN_LED_EN_GPIO_Port GPIOA
#define BTN_BACK_Pin GPIO_PIN_12
#define BTN_BACK_GPIO_Port GPIOA
#define BTN_BACK_EXTI_IRQn EXTI15_10_IRQn
#define T_SWDIO_Pin GPIO_PIN_13
#define T_SWDIO_GPIO_Port GPIOA
#define T_SWCLK_Pin GPIO_PIN_14
#define T_SWCLK_GPIO_Port GPIOA
#define T_SWO_Pin GPIO_PIN_3
#define T_SWO_GPIO_Port GPIOB
#define BTN_DOWN_Pin GPIO_PIN_4
#define BTN_DOWN_GPIO_Port GPIOB
#define BTN_DOWN_EXTI_IRQn EXTI4_IRQn
#define TFT_RST_Pin GPIO_PIN_5
#define TFT_RST_GPIO_Port GPIOB
#define TFT_DC_Pin GPIO_PIN_6
#define TFT_DC_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
