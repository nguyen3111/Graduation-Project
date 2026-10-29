/**
 * @file    battery_manager.c
 * @brief   Đọc điện áp pin thực, tính SoC, phát hiện low battery
 *
 * Mạch chia áp: R1=22k, R2=10k
 *   V_pack = V_adc × (R1 + R2) / R2 = V_adc × 3.2
 *
 * SoC tính theo Lookup Table thực nghiệm của pin 18650 2S
 *
 * @note  MISRA C:2012 compliant (basic rules)
 */

#include "battery_manager.h"
#include "ecu_data.h"
#include "app_config.h"
#include "input_manager.h"
#include "vehicle_types.h"
#include <stdio.h>

/* =========================================================
 * SECTION 1: SOC LOOKUP TABLE
 * Ánh xạ điện áp pack (mV) → SoC (%)
 * Dựa trên đặc tuyến discharge thực nghiệm pin 18650
 * Sắp xếp từ cao → thấp
 * ========================================================= */

typedef struct
{
    uint16_t voltage_mv;
    uint8_t  soc_pct;
} SocPoint_t;

static const SocPoint_t SOC_TABLE[] =
{
    { 8400U, 100U },
    { 8200U,  90U },
    { 8000U,  80U },
    { 7800U,  70U },
    { 7600U,  60U },
    { 7400U,  50U },
    { 7200U,  40U },
    { 7000U,  30U },
    { 6800U,  20U },
    { 6600U,  10U },
    { 6400U,   0U }
};

#define SOC_TABLE_SIZE  (sizeof(SOC_TABLE) / sizeof(SOC_TABLE[0]))

/* =========================================================
 * SECTION 2: PRIVATE FUNCTIONS
 * ========================================================= */

/**
 * @brief Chuyển đổi ADC raw → điện áp pack thực (mV)
 *
 * Công thức:
 *   V_adc_mv  = raw × VREF / ADC_MAX
 *   V_pack_mv = V_adc_mv × VDIV_FACTOR_X100 / 100
 */
static uint16_t ADC_ToPackVoltage_mV(uint16_t adc_raw)
{
    /* Dùng uint32_t trung gian tránh overflow */
    uint32_t v_adc_mv  = ((uint32_t)adc_raw * (uint32_t)ADC_VREF_MV)
                         / (uint32_t)ADC_MAX_VALUE;
    uint32_t v_pack_mv = (v_adc_mv * (uint32_t)VDIV_FACTOR_X100) / 100U;

    /* Clamp về uint16_t */
    if (v_pack_mv > 0xFFFFU) { v_pack_mv = 0xFFFFU; }

    return (uint16_t)v_pack_mv;
}

/**
 * @brief Nội suy tuyến tính SoC từ điện áp pack
 * @param voltage_mv  Điện áp pack (mV)
 * @return SoC 0-100%
 */
static uint8_t Voltage_ToSoC(uint16_t voltage_mv)
{
    /* Trên ngưỡng max → đầy 100% */
    if (voltage_mv >= SOC_TABLE[0].voltage_mv)
    {
        return 100U;
    }

    /* Dưới ngưỡng min → cạn 0% */
    if (voltage_mv <= SOC_TABLE[SOC_TABLE_SIZE - 1U].voltage_mv)
    {
        return 0U;
    }

    /* Tìm đoạn chứa voltage_mv và nội suy tuyến tính */
    for (uint8_t i = 0U; i < (SOC_TABLE_SIZE - 1U); i++)
    {
        if ((voltage_mv <= SOC_TABLE[i].voltage_mv) &&
            (voltage_mv >= SOC_TABLE[i + 1U].voltage_mv))
        {
            uint16_t v_hi = SOC_TABLE[i].voltage_mv;
            uint16_t v_lo = SOC_TABLE[i + 1U].voltage_mv;
            uint8_t  s_hi = SOC_TABLE[i].soc_pct;
            uint8_t  s_lo = SOC_TABLE[i + 1U].soc_pct;

            uint32_t soc = (uint32_t)s_lo +
                           ((uint32_t)(voltage_mv - v_lo) *
                            (uint32_t)(s_hi - s_lo)) /
                           (uint32_t)(v_hi - v_lo);
            return (uint8_t)soc;
        }
    }

    return 0U;
}

/* =========================================================
 * SECTION 3: PUBLIC FUNCTIONS
 * ========================================================= */

void BatteryManager_Init(void)
{
    /* ADC DMA đã được start trong InputManager_Init() */
}

/**
 * @brief Task Battery Manager — chu kỳ 100ms
 *
 * Luồng:
 *   1. Đọc batt_raw từ InputManager (atomic read, không cần mutex)
 *   2. Tính pack voltage và SoC (ngoài mutex — giữ mutex ngắn nhất)
 *   3. Kiểm tra low battery warning
 *   4. Ghi kết quả vào g_ecu_data qua xEcuMutex
 *   5. Log mỗi 1 giây (10 × 100ms)
 */
void Task_BatteryManager(void *argument)
{
    uint32_t tick    = osKernelGetTickCount();
    uint32_t log_cnt = 0U;

    for (;;)
    {
        /* --- 1. Đọc ADC raw — atomic, không cần mutex --- */
        uint16_t batt_raw = InputManager_GetBattRaw();

        /* --- 2. Tính toán ngoài mutex --- */
        uint16_t pack_mv     = ADC_ToPackVoltage_mV(batt_raw);
        uint8_t  soc         = Voltage_ToSoC(pack_mv);
        bool     low_warning = (soc <= BATT_LOW_WARNING_PCT);

        /* --- 3. Ghi kết quả vào g_ecu_data --- */
        (void)osMutexAcquire(xEcuMutex, osWaitForever);
        g_ecu_data.batt_voltage    = (float)pack_mv / 1000.0f;
        g_ecu_data.soc_pct         = soc;
        g_ecu_data.batt_low_warning = low_warning;
        (void)osMutexRelease(xEcuMutex);

        /* --- 4. Log mỗi 1 giây --- */
        log_cnt++;
        if (log_cnt >= 10U)
        {
            log_cnt = 0U;

            /* Snapshot để print — không giữ mutex khi printf */
            (void)osMutexAcquire(xEcuMutex, osWaitForever);
            EcuData_t snap = g_ecu_data;
            (void)osMutexRelease(xEcuMutex);

            printf("[ECU] Accel:%3u%%  Batt:%.2fV  SoC:%3u%%%s"
                   "  Speed:%.1fkm/h  PWM:%4u\r\n",
                   snap.accel_pct,
                   snap.batt_voltage,
                   snap.soc_pct,
                   snap.batt_low_warning ? "!" : " ",
                   snap.speed_kmh,
                   snap.motor_pwm);

            printf("[ECU] Key:%u Brake:%u Gear:%u Mode:%u"
                   " Chg:%u Fault:%u\r\n",
                   (unsigned)snap.key_on,
                   (unsigned)snap.brake_pressed,
                   (uint8_t)snap.gear,
                   (uint8_t)snap.drive_mode,
                   (unsigned)snap.charging_detect,
                   (unsigned)snap.fault_inject);
        }

        tick += TASK_PERIOD_BATTERY_MS;
        osDelayUntil(tick);
    }
}
