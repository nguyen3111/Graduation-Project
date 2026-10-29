/**
 * @file    input_manager.h
 * @brief   Hardware input reading — ADC analog + debounced digital inputs
 */

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "vehicle_types.h"
#include "cmsis_os.h"   /* FreeRTOS CMSIS wrapper */

/* Queue handle — InputTask ghi vào, StateTask/MotorTask đọc ra */
extern osMessageQueueId_t xInputQueue;

/**
 * @brief Khởi tạo input manager (gọi 1 lần trước khi start scheduler)
 */
void InputManager_Init(void);

/**
 * @brief FreeRTOS Task function — đăng ký với osThreadNew()
 * @param argument: không dùng, để NULL
 */
void Task_InputScan(void *argument);

/**
 * @brief Lấy ADC raw của accelerator từ DMA buffer
 *
 * @note  Thread-safe theo nghĩa: đọc uint16_t là atomic trên Cortex-M3.
 *        Giá trị có thể thay đổi giữa 2 lần gọi (DMA đang update liên tục)
 *        nhưng mỗi lần đọc là nhất quán (không bao giờ đọc half-written value).
 *
 * @return ADC raw 0–4095, tương ứng với 0–3.3V tại PA4
 */
uint16_t InputManager_GetAccelRaw(void);

/**
 * @brief Lấy ADC raw của battery voltage từ DMA buffer
 *
 * @note  Tương tự InputManager_GetAccelRaw.
 *        Giá trị này cần được convert qua mạch chia áp để ra V_pack thực tế.
 *
 * @return ADC raw 0–4095, tương ứng với điện áp tại PA2 (sau chia áp R1=22k, R2=10k)
 */
uint16_t InputManager_GetBattRaw(void);

#endif /* INPUT_MANAGER_H */
