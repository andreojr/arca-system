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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define NFC_INT_Pin GPIO_PIN_1
#define NFC_INT_GPIO_Port GPIOA
#define NFC_INT_EXTI_IRQn EXTI1_IRQn
#define NFC_NSS_Pin GPIO_PIN_3
#define NFC_NSS_GPIO_Port GPIOA
#define DOOR_ANALOG_PRESS_Pin GPIO_PIN_4
#define DOOR_ANALOG_PRESS_GPIO_Port GPIOA
#define DHT11_DATA_Pin GPIO_PIN_0
#define DHT11_DATA_GPIO_Port GPIOB
#define IHM_LED_Pin GPIO_PIN_1
#define IHM_LED_GPIO_Port GPIOB
#define NFC_RESET_Pin GPIO_PIN_2
#define NFC_RESET_GPIO_Port GPIOB
#define LED_DENIED_Pin GPIO_PIN_12
#define LED_DENIED_GPIO_Port GPIOB
#define LED_GRANTED_Pin GPIO_PIN_13
#define LED_GRANTED_GPIO_Port GPIOB
#define INT_CC1101_Pin GPIO_PIN_15
#define INT_CC1101_GPIO_Port GPIOB
#define INT_CC1101_EXTI_IRQn EXTI15_10_IRQn
#define IHM_RESET_Pin GPIO_PIN_8
#define IHM_RESET_GPIO_Port GPIOA
#define NSS_CC1101_Pin GPIO_PIN_10
#define NSS_CC1101_GPIO_Port GPIOA
#define NSS_IHM_Pin GPIO_PIN_11
#define NSS_IHM_GPIO_Port GPIOA
#define IHM_DC_Pin GPIO_PIN_12
#define IHM_DC_GPIO_Port GPIOA
#define BUZZER_Pin GPIO_PIN_3
#define BUZZER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
