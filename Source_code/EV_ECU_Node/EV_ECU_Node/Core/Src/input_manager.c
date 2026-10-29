/**
 * @file    input_manager.c
 * @brief   Đọc và xử lý toàn bộ hardware inputs
 *
 * Chức năng:
 *   - Đọc ADC (accelerator + battery) bằng DMA circular
 *   - Đọc digital inputs với software debounce
 *   - Gear và Drive Mode dùng LATCHING logic:
 *       Bấm 1 lần → chuyển chế độ → giữ nguyên khi nhả nút
 *   - Đóng gói vào HardwareInput_t và gửi lên xInputQueue mỗi 10ms
 *   - Cập nhật g_ecu_data với hardware inputs mới nhất
 *
 * @note  MISRA C:2012 compliant (basic rules)
 */

#include "input_manager.h"
#include "app_config.h"
#include "vehicle_types.h"
#include "ecu_data.h"
#include "cmsis_os.h"
#include <string.h>

/* =========================================================
 * SECTION 1: PRIVATE DEFINES
 * ========================================================= */

/** Số lần đọc liên tiếp cùng giá trị để xác nhận debounce
 *  10ms × 3 = 30ms debounce time */
#define DEBOUNCE_COUNT      (3U)

/** Queue capacity — 2 item đủ dùng, chỉ có 1 consumer */
#define INPUT_QUEUE_SIZE    (2U)

/* =========================================================
 * SECTION 2: PRIVATE TYPES
 * ========================================================= */

/**
 * @brief Trạng thái debounce cho 1 nút nhấn
 */
typedef struct
{
    uint8_t count;      /* Số lần đọc liên tiếp cùng giá trị */
    bool    stable;     /* Giá trị đã ổn định                 */
    bool    last_raw;   /* Giá trị raw lần đọc trước          */
} DebounceState_t;

/* =========================================================
 * SECTION 3: PRIVATE VARIABLES
 * ========================================================= */

/** Buffer ADC DMA circular — tự động cập nhật liên tục */
static volatile uint16_t s_adc_buf[ADC_NUM_CHANNELS] = {0U, 0U};

/** Debounce state cho từng nút */
static DebounceState_t s_db_key      = {0U, false, false};
static DebounceState_t s_db_brake    = {0U, false, false};
static DebounceState_t s_db_charging = {0U, false, false};
static DebounceState_t s_db_fault    = {0U, false, false};
static DebounceState_t s_db_gearP    = {0U, false, false};
static DebounceState_t s_db_gearR    = {0U, false, false};
static DebounceState_t s_db_gearN    = {0U, false, false};
static DebounceState_t s_db_gearD    = {0U, false, false};
static DebounceState_t s_db_modeEco  = {0U, false, false};
static DebounceState_t s_db_modeNorm = {0U, false, false};
static DebounceState_t s_db_modeSpt  = {0U, false, false};

/**
 * @brief Latching state cho Gear và Drive Mode
 *        Bấm 1 lần → chuyển → giữ nguyên khi nhả nút
 */
static GearPosition_t s_gear_current      = GEAR_N;
static DriveMode_t    s_drivemode_current = DRIVE_MODE_NORMAL;

/** Previous debounce state — để phát hiện rising edge */
static bool s_gear_p_prev    = false;
static bool s_gear_r_prev    = false;
static bool s_gear_n_prev    = false;
static bool s_gear_d_prev    = false;
static bool s_mode_eco_prev  = false;
static bool s_mode_norm_prev = false;
static bool s_mode_spt_prev  = false;

/* =========================================================
 * SECTION 4: PUBLIC VARIABLES
 * ========================================================= */

osMessageQueueId_t xInputQueue = NULL;

/* =========================================================
 * SECTION 5: PRIVATE FUNCTION DECLARATIONS
 * ========================================================= */

static void           ADC_StartDMA(void);
static uint16_t       ADC_GetChannel(uint8_t rank);
static bool           Debounce_Update(DebounceState_t *db, bool raw_active);
static GearPosition_t ReadGear(void);
static DriveMode_t    ReadDriveMode(void);

/* =========================================================
 * SECTION 6: PRIVATE FUNCTIONS
 * ========================================================= */

static void ADC_StartDMA(void)
{
    HAL_ADC_Start_DMA(&ADC_HANDLE,
                      (uint32_t *)s_adc_buf,
                      ADC_NUM_CHANNELS);
}

static uint16_t ADC_GetChannel(uint8_t rank)
{
    if (rank >= ADC_NUM_CHANNELS) { return 0U; }
    return s_adc_buf[rank];
}

/**
 * @brief Debounce: đếm DEBOUNCE_COUNT lần liên tiếp cùng giá trị
 *        mới xác nhận thay đổi trạng thái
 */
static bool Debounce_Update(DebounceState_t *db, bool raw_active)
{
    if (raw_active == db->last_raw)
    {
        if (db->count < DEBOUNCE_COUNT) { db->count++; }
        if (db->count >= DEBOUNCE_COUNT) { db->stable = raw_active; }
    }
    else
    {
        db->count    = 0U;
        db->last_raw = raw_active;
    }
    return db->stable;
}

/**
 * @brief Đọc Gear với LATCHING logic
 *
 * Phát hiện rising edge (nút vừa được nhấn) → cập nhật s_gear_current.
 * Khi nhả nút, s_gear_current giữ nguyên giá trị đã chọn.
 *
 * Ưu tiên nếu nhiều nút nhấn cùng lúc: D > R > P > N
 */
static GearPosition_t ReadGear(void)
{
    /* Đọc trạng thái debounce hiện tại */
    bool p = Debounce_Update(&s_db_gearP,
                 HAL_GPIO_ReadPin(GEAR_P_PORT, GEAR_P_PIN) == GPIO_PIN_RESET);
    bool r = Debounce_Update(&s_db_gearR,
                 HAL_GPIO_ReadPin(GEAR_R_PORT, GEAR_R_PIN) == GPIO_PIN_RESET);
    bool n = Debounce_Update(&s_db_gearN,
                 HAL_GPIO_ReadPin(GEAR_N_PORT, GEAR_N_PIN) == GPIO_PIN_RESET);
    bool d = Debounce_Update(&s_db_gearD,
                 HAL_GPIO_ReadPin(GEAR_D_PORT, GEAR_D_PIN) == GPIO_PIN_RESET);

    /* Rising edge detection — chỉ cập nhật khi nút VỪA được nhấn
     * (false → true transition sau debounce) */
    if (d && !s_gear_d_prev) { s_gear_current = GEAR_D; }
    if (r && !s_gear_r_prev) { s_gear_current = GEAR_R; }
    if (p && !s_gear_p_prev) { s_gear_current = GEAR_P; }
    if (n && !s_gear_n_prev) { s_gear_current = GEAR_N; }

    /* Lưu trạng thái để so sánh lần sau */
    s_gear_p_prev = p;
    s_gear_r_prev = r;
    s_gear_n_prev = n;
    s_gear_d_prev = d;

    return s_gear_current;
}

/**
 * @brief Đọc Drive Mode với LATCHING logic
 *
 * Bấm nút ECO/NORMAL/SPORT 1 lần → chuyển mode → giữ khi nhả.
 * Ưu tiên nếu nhiều nút: SPORT > ECO > NORMAL
 */
static DriveMode_t ReadDriveMode(void)
{
    bool eco   = Debounce_Update(&s_db_modeEco,
                     HAL_GPIO_ReadPin(MODE_ECO_PORT,    MODE_ECO_PIN)    == GPIO_PIN_RESET);
    bool norm  = Debounce_Update(&s_db_modeNorm,
                     HAL_GPIO_ReadPin(MODE_NORMAL_PORT, MODE_NORMAL_PIN) == GPIO_PIN_RESET);
    bool sport = Debounce_Update(&s_db_modeSpt,
                     HAL_GPIO_ReadPin(MODE_SPORT_PORT,  MODE_SPORT_PIN)  == GPIO_PIN_RESET);

    /* Rising edge detection */
    if (sport && !s_mode_spt_prev)  { s_drivemode_current = DRIVE_MODE_SPORT;  }
    if (eco   && !s_mode_eco_prev)  { s_drivemode_current = DRIVE_MODE_ECO;    }
    if (norm  && !s_mode_norm_prev) { s_drivemode_current = DRIVE_MODE_NORMAL; }

    /* Lưu trạng thái */
    s_mode_eco_prev  = eco;
    s_mode_norm_prev = norm;
    s_mode_spt_prev  = sport;

    return s_drivemode_current;
}

/* =========================================================
 * SECTION 7: PUBLIC FUNCTIONS
 * ========================================================= */

void InputManager_Init(void)
{
    xInputQueue = osMessageQueueNew(INPUT_QUEUE_SIZE,
                                    sizeof(HardwareInput_t),
                                    NULL);
    ADC_StartDMA();
}

uint16_t InputManager_GetAccelRaw(void)
{
    return s_adc_buf[ADC_RANK_ACCEL];
}

uint16_t InputManager_GetBattRaw(void)
{
    return s_adc_buf[ADC_RANK_BATT];
}

/**
 * @brief Task Input Scan — chu kỳ 10ms
 *
 * Luồng:
 *   1. Đọc ADC + tính accel_pct
 *   2. Debounce digital inputs (key, brake, charging, fault)
 *   3. Đọc Gear (latching) và Drive Mode (latching)
 *   4. Push vào xInputQueue — overwrite nếu đầy
 *   5. Cập nhật g_ecu_data
 */
void Task_InputScan(void *argument)
{
    uint32_t tick = osKernelGetTickCount();

    for (;;)
    {
        HardwareInput_t input;
        (void)memset(&input, 0, sizeof(HardwareInput_t));

        /* --- 1. ADC --- */
        uint16_t accel_raw = ADC_GetChannel(ADC_RANK_ACCEL);
        input.batt_raw     = ADC_GetChannel(ADC_RANK_BATT);
        input.accel_pct    = (uint8_t)(((uint32_t)accel_raw * 100U)
                                        / (uint32_t)ADC_MAX_VALUE);

        /* --- 2. Digital inputs (momentary — phản ánh trạng thái tức thời) --- */
        input.key_on = Debounce_Update(&s_db_key,
            HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_RESET);

        input.brake_pressed = Debounce_Update(&s_db_brake,
            HAL_GPIO_ReadPin(BRAKE_PORT, BRAKE_PIN) == GPIO_PIN_RESET);

        input.charging_detect = Debounce_Update(&s_db_charging,
            HAL_GPIO_ReadPin(CHARGING_PORT, CHARGING_PIN) == GPIO_PIN_RESET);

        input.fault_inject = Debounce_Update(&s_db_fault,
            HAL_GPIO_ReadPin(FAULT_INJECT_PORT, FAULT_INJECT_PIN) == GPIO_PIN_RESET);

        /* --- 3. Gear và Mode (latching — bấm 1 lần để chuyển) --- */
        input.gear       = ReadGear();
        input.drive_mode = ReadDriveMode();

        /* --- 4. Push queue — overwrite nếu đầy --- */
        if (osMessageQueueGetSpace(xInputQueue) == 0U)
        {
            HardwareInput_t dummy;
            (void)osMessageQueueGet(xInputQueue, &dummy, NULL, 0U);
        }
        (void)osMessageQueuePut(xInputQueue, &input, 0U, 0U);

        /* --- 5. Cập nhật g_ecu_data --- */
        (void)osMutexAcquire(xEcuMutex, osWaitForever);
        g_ecu_data.accel_pct       = input.accel_pct;
        g_ecu_data.key_on          = input.key_on;
        g_ecu_data.brake_pressed   = input.brake_pressed;
        g_ecu_data.charging_detect = input.charging_detect;
        g_ecu_data.fault_inject    = input.fault_inject;
        g_ecu_data.gear            = input.gear;
        g_ecu_data.drive_mode      = input.drive_mode;
        (void)osMutexRelease(xEcuMutex);

        tick += TASK_PERIOD_INPUT_MS;
        osDelayUntil(tick);
    }
}
