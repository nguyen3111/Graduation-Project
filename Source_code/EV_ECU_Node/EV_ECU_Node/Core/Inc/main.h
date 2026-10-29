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
#include "stm32f1xx_hal.h"

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
//#define GEAR_P_Pin GPIO_PIN_5
//#define GEAR_P_GPIO_Port GPIOA
//#define GEAR_R_Pin GPIO_PIN_6
//#define GEAR_R_GPIO_Port GPIOA
//#define GEAR_N_Pin GPIO_PIN_7
//#define GEAR_N_GPIO_Port GPIOA
//#define MOTOR_AIN1_Pin GPIO_PIN_0
//#define MOTOR_AIN1_GPIO_Port GPIOB
//#define MOTOR_AIN2_Pin GPIO_PIN_1
//#define MOTOR_AIN2_GPIO_Port GPIOB
//#define MOTOR_STBY_Pin GPIO_PIN_11
//#define MOTOR_STBY_GPIO_Port GPIOB
//#define BRAKE_Pin GPIO_PIN_12
//#define BRAKE_GPIO_Port GPIOB
//#define KEY_Pin GPIO_PIN_13
//#define KEY_GPIO_Port GPIOB
//#define CHARGING_DETECT_Pin GPIO_PIN_14
//#define CHARGING_DETECT_GPIO_Port GPIOB
//#define GEAR_D_Pin GPIO_PIN_6
//#define GEAR_D_GPIO_Port GPIOB
//#define MODE_ECO_Pin GPIO_PIN_7
//#define MODE_ECO_GPIO_Port GPIOB
//#define MODE_NORMAL_Pin GPIO_PIN_8
//#define MODE_NORMAL_GPIO_Port GPIOB
//#define MODE_SPORT_Pin GPIO_PIN_9
//#define MODE_SPORT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
