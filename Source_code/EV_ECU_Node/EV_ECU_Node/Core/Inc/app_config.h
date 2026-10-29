/**
 * @file    app_config.h
 * @brief   Hardware pin mapping và cấu hình ngoại vi tập trung
 *
 * @note  Tất cả định nghĩa phần cứng chỉ nằm ở đây.
 *        Khi thay đổi phần cứng, chỉ cần sửa file này.
 *        Pin defines (GPIO_PIN_x, GPIOx) lấy từ main.h (CubeMX).
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "stm32f1xx_hal.h"
#include "main.h"   /* GPIO defines từ CubeMX User Labels */

/* =========================================================
 * SECTION 1: ADC
 * PA4 — Accelerator biến trở (Rank 1)
 * PA2 — Battery voltage sau mạch chia áp (Rank 2)
 * ========================================================= */
extern ADC_HandleTypeDef hadc1;

#define ADC_HANDLE          hadc1

/* =========================================================
 * SECTION 2: MOTOR PWM — TIM1 Channel 1 → PA8
 * ========================================================= */
extern TIM_HandleTypeDef htim1;

#define MOTOR_PWM_TIM       htim1
#define MOTOR_PWM_CHANNEL   TIM_CHANNEL_1
#define MOTOR_PWM_ARR       (999U)   /* ARR=999 → 1000 steps, 1kHz PWM */

/* TB6612 direction control */
#define MOTOR_AIN1_PORT     GPIOB
#define MOTOR_AIN1_PIN      GPIO_PIN_0

#define MOTOR_AIN2_PORT     GPIOB
#define MOTOR_AIN2_PIN      GPIO_PIN_1

#define MOTOR_STBY_PORT     GPIOB
#define MOTOR_STBY_PIN      GPIO_PIN_11

/* =========================================================
 * SECTION 3: ENCODER — TIM2 Encoder Mode
 * PA0 = TIM2_CH1 (Encoder A)
 * PA1 = TIM2_CH2 (Encoder B)
 * ========================================================= */
extern TIM_HandleTypeDef htim2;

#define ENCODER_TIM         htim2
#define ENCODER_TIM_PERIOD  (65535U)   /* TIM2 16-bit counter max */

/* =========================================================
 * SECTION 4: CAN — PA11 CAN_RX, PA12 CAN_TX
 * ========================================================= */
extern CAN_HandleTypeDef hcan;

#define CAN_HANDLE          hcan
#define CAN_BITRATE         500000U    /* 500 kbps */

/* =========================================================
 * SECTION 5: DIGITAL INPUTS
 * Tất cả INPUT_PULLUP → active-low (GPIO_PIN_RESET = active)
 * ========================================================= */

/* Key / Power switch — PB13 */
#define KEY_PORT            GPIOB
#define KEY_PIN             GPIO_PIN_13

/* Brake — PB12 */
#define BRAKE_PORT          GPIOB
#define BRAKE_PIN           GPIO_PIN_12

/* Gear selector */
#define GEAR_P_PORT         GPIOA
#define GEAR_P_PIN          GPIO_PIN_5

#define GEAR_R_PORT         GPIOA
#define GEAR_R_PIN          GPIO_PIN_6

#define GEAR_N_PORT         GPIOA
#define GEAR_N_PIN          GPIO_PIN_7

#define GEAR_D_PORT         GPIOB
#define GEAR_D_PIN          GPIO_PIN_6

/* Drive mode */
#define MODE_ECO_PORT       GPIOB
#define MODE_ECO_PIN        GPIO_PIN_7

#define MODE_NORMAL_PORT    GPIOB
#define MODE_NORMAL_PIN     GPIO_PIN_8

#define MODE_SPORT_PORT     GPIOB
#define MODE_SPORT_PIN      GPIO_PIN_9

/* Charging detect — PB14 */
#define CHARGING_PORT       GPIOB
#define CHARGING_PIN        GPIO_PIN_14

/* Fault inject (test button) — PB15 */
#define FAULT_INJECT_PORT   GPIOB
#define FAULT_INJECT_PIN    GPIO_PIN_15

/* =========================================================
 * SECTION 6: UART DEBUG — PA9 TX, PA10 RX
 * ========================================================= */
extern UART_HandleTypeDef huart1;

#define DEBUG_UART          huart1
#define DEBUG_UART_BAUDRATE 115200U

#endif /* APP_CONFIG_H */
