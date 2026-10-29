/**
 * @file    state_machine.h
 * @brief   Vehicle state machine — quản lý trạng thái toàn hệ thống
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "vehicle_types.h"
#include "cmsis_os.h"

/**
 * @brief Mutex bảo vệ g_vehicle — share với tất cả task
 *        Task nào đọc/ghi g_vehicle đều phải acquire mutex này
 */
extern osMutexId_t xVehicleMutex;

/**
 * @brief Global vehicle data — single source of truth
 *        KHÔNG đọc trực tiếp mà không có mutex
 */
extern VehicleData_t g_vehicle;

/**
 * @brief Khởi tạo state machine (gọi trước osKernelStart)
 */
void StateMachine_Init(void);

/**
 * @brief FreeRTOS Task
 */
void Task_StateMachine(void *argument);

#endif /* STATE_MACHINE_H */
