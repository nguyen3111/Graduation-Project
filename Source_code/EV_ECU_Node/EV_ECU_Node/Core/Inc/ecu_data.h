/**
 * @file    ecu_data.h
 * @brief   Global ECU data và VCU command — thay thế g_vehicle cũ
 *
 * Thay vì 1 struct g_vehicle dùng chung cho mọi thứ,
 * tách rõ thành 2 chiều:
 *
 *   g_ecu_data   — STM32 sở hữu, ghi bởi các task nội bộ
 *                  bảo vệ bởi xEcuMutex
 *
 *   g_vcu_cmd    — VCU (Pi) sở hữu, STM32 chỉ đọc và thực thi
 *                  bảo vệ bởi xVcuCmdMutex
 *
 * @note  Mọi file cần đọc/ghi g_ecu_data hoặc g_vcu_cmd
 *        đều include file này thay vì "state_machine.h" cũ
 */

#ifndef ECU_DATA_H
#define ECU_DATA_H

#include "vehicle_types.h"
#include "cmsis_os.h"

/* =========================================================
 * ECU DATA — ghi bởi STM32 tasks, đọc bởi CAN Transmit
 * ========================================================= */

/** Global ECU data — single source of truth trên STM32 */
extern EcuData_t        g_ecu_data;

/** Mutex bảo vệ g_ecu_data */
extern osMutexId_t      xEcuMutex;

/* =========================================================
 * VCU COMMAND — nhận từ Pi qua CAN 0x200
 * ========================================================= */

/** Lệnh mới nhất từ VCU */
extern VcuCommand_t     g_vcu_cmd;

/** Mutex bảo vệ g_vcu_cmd */
extern osMutexId_t      xVcuCmdMutex;

/**
 * @brief Timestamp lần cuối nhận lệnh VCU (osKernelGetTickCount)
 *        Dùng để phát hiện VCU timeout → dừng motor an toàn
 */
extern uint32_t         g_vcu_last_tick;

/* =========================================================
 * KHỞI TẠO — gọi 1 lần trong App_Init() trước mọi task
 * ========================================================= */

/**
 * @brief Khởi tạo g_ecu_data, g_vcu_cmd và 2 mutex
 */
void EcuData_Init(void);

#endif /* ECU_DATA_H */
