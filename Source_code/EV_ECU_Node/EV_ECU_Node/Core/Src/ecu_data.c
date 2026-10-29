/**
 * @file    ecu_data.c
 * @brief   Định nghĩa và khởi tạo global ECU data + VCU command
 */

#include "ecu_data.h"
#include <string.h>

/* =========================================================
 * ĐỊNH NGHĨA GLOBAL VARIABLES
 * ========================================================= */

EcuData_t    g_ecu_data     = {0};
osMutexId_t  xEcuMutex      = NULL;

VcuCommand_t g_vcu_cmd      = {0};
osMutexId_t  xVcuCmdMutex   = NULL;
uint32_t     g_vcu_last_tick = 0U;

/* =========================================================
 * KHỞI TẠO
 * ========================================================= */

void EcuData_Init(void)
{
    /* Reset toàn bộ data về 0 / safe defaults */
    (void)memset(&g_ecu_data, 0, sizeof(EcuData_t));
    g_ecu_data.gear       = GEAR_N;
    g_ecu_data.drive_mode = DRIVE_MODE_NORMAL;

    /* VCU command mặc định: motor OFF, chờ lệnh từ Pi */
    (void)memset(&g_vcu_cmd, 0, sizeof(VcuCommand_t));
    g_vcu_cmd.motor_enable    = false;
    g_vcu_cmd.pwm_target_pct  = 0U;
    g_vcu_cmd.vehicle_state   = (uint8_t)VEHICLE_STATE_OFF;

    /* Timestamp khởi tạo */
    g_vcu_last_tick = 0U;

    /* Tạo mutex */
    xEcuMutex    = osMutexNew(NULL);
    xVcuCmdMutex = osMutexNew(NULL);
}
