/**
 * @file    motor_control.c
 * @brief   Thực thi lệnh motor từ VCU + đo tốc độ encoder
 *
 * Kiến trúc:
 *   KHÔNG tự quyết định motor có chạy không.
 *   Chỉ đọc g_vcu_cmd và thực thi PWM theo đúng lệnh.
 *
 * Safety duy nhất trên STM32:
 *   Nếu không nhận lệnh VCU quá VCU_CMD_TIMEOUT_MS → dừng motor.
 *   last_vcu_tick == 0 → VCU chưa gửi lệnh nào kể từ boot
 *   → motor không chạy cho đến khi VCU xác nhận (safety-by-default).
 *
 * @note  MISRA C:2012 compliant (basic rules)
 */

#include "motor_control.h"
#include "ecu_data.h"
#include "app_config.h"
#include "vehicle_types.h"

/* =========================================================
 * SECTION 1: PRIVATE VARIABLES
 * ========================================================= */

/** Giá trị encoder lần trước — để tính delta xung */
static int32_t s_encoder_last = 0;

/* =========================================================
 * SECTION 2: PRIVATE FUNCTIONS
 * ========================================================= */

/**
 * @brief Set PWM và chiều quay cho motor
 * @param pwm_value  0 đến MOTOR_PWM_MAX
 * @param forward    true = tiến, false = lùi
 */
static void Motor_SetPWM(uint16_t pwm_value, bool forward)
{
    /* Clamp về giới hạn an toàn */
    if (pwm_value > MOTOR_PWM_MAX)
    {
        pwm_value = MOTOR_PWM_MAX;
    }

    /* Set direction qua TB6612 */
    if (forward)
    {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
    }

    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL, pwm_value);
}

/**
 * @brief Dừng motor — coast mode (không phanh động)
 */
static void Motor_Stop(void)
{
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL, 0U);
}

/**
 * @brief Tính tốc độ và quãng đường từ encoder
 *
 * @param period_ms  Chu kỳ task (ms) — dùng để tính RPM chính xác
 * @param out_speed  [out] Tốc độ km/h
 * @param out_dist   [out] Quãng đường di chuyển trong chu kỳ (mm)
 */
static void Motor_CalcSpeed(uint32_t  period_ms,
                             float    *out_speed,
                             uint32_t *out_dist)
{
    /* Đọc encoder counter */
    int32_t encoder_now = (int32_t)__HAL_TIM_GET_COUNTER(&ENCODER_TIM);
    int32_t delta       = encoder_now - s_encoder_last;

    /* Xử lý wrap-around 16-bit TIM2 (0–65535) */
    if (delta > (int32_t)(ENCODER_TIM_PERIOD / 2U))
    {
        delta -= (int32_t)ENCODER_TIM_PERIOD;
    }
    else if (delta < -(int32_t)(ENCODER_TIM_PERIOD / 2U))
    {
        delta += (int32_t)ENCODER_TIM_PERIOD;
    }

    s_encoder_last = encoder_now;

    uint32_t abs_delta        = (delta >= 0) ? (uint32_t)delta
                                              : (uint32_t)(-delta);
    uint32_t pulses_per_wheel = (uint32_t)ENCODER_PPR *
                                (uint32_t)GEAR_RATIO * 2U;

    if (pulses_per_wheel > 0U)
    {
        /*
         * Tính tốc độ:
         *   pulses/period → rev/s → wheel_rev/s → mm/s → km/h
         */
        float rev_per_sec = ((float)abs_delta / (float)pulses_per_wheel)
                            * (1000.0f / (float)period_ms);
        float mm_per_sec  = rev_per_sec * (float)WHEEL_CIRCUMFERENCE_MM;

        *out_speed = mm_per_sec * 3.6f / 1000.0f;
        *out_dist  = (abs_delta * WHEEL_CIRCUMFERENCE_MM) / pulses_per_wheel;
    }
    else
    {
        *out_speed = 0.0f;
        *out_dist  = 0U;
    }
}

/* =========================================================
 * SECTION 3: PUBLIC FUNCTIONS
 * ========================================================= */

void MotorControl_Init(void)
{
    /* Khởi động PWM timer */
    HAL_TIM_PWM_Start(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL);

    /* Khởi động Encoder timer */
    HAL_TIM_Encoder_Start(&ENCODER_TIM, TIM_CHANNEL_ALL);

    /* Enable TB6612 (thoát Standby) */
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);

    /* Motor dừng an toàn khi khởi động */
    Motor_Stop();
}

/**
 * @brief Task Motor Control — chu kỳ 5ms (priority cao nhất)
 *
 * Luồng:
 *   1. Snapshot g_vcu_cmd — giữ mutex ngắn
 *   2. Kiểm tra VCU timeout watchdog
 *   3. Tính tốc độ từ encoder (không cần mutex)
 *   4. Thực thi PWM theo lệnh VCU
 *   5. Ghi motor_pwm + speed + odometer vào g_ecu_data — 1 lần acquire
 */
void Task_MotorControl(void *argument)
{
    uint32_t tick = osKernelGetTickCount();

    for (;;)
    {
        /* --- 1. Snapshot lệnh VCU --- */
        (void)osMutexAcquire(xVcuCmdMutex, osWaitForever);
        VcuCommand_t cmd           = g_vcu_cmd;
        uint32_t     last_vcu_tick = g_vcu_last_tick;
        (void)osMutexRelease(xVcuCmdMutex);

        /* --- 2. VCU Timeout Watchdog ---
         * last_vcu_tick == 0 → VCU chưa gửi lệnh nào kể từ boot
         *                    → motor không chạy (safety-by-default)
         * timeout quá hạn  → mất liên lạc VCU → dừng motor ngay  */
        uint32_t now           = osKernelGetTickCount();
        bool     vcu_timed_out = (last_vcu_tick == 0U) ||
                                 ((now - last_vcu_tick) > VCU_CMD_TIMEOUT_MS);

        /* --- 3. Tính tốc độ từ encoder (ngoài mutex) --- */
        float    speed_kmh = 0.0f;
        uint32_t dist_mm   = 0U;
        Motor_CalcSpeed(TASK_PERIOD_MOTOR_MS, &speed_kmh, &dist_mm);

        /* --- 4. Thực thi lệnh VCU --- */
        uint16_t pwm_out = 0U;

        if (vcu_timed_out)
        {
            /* Mất liên lạc VCU → dừng motor ngay */
            Motor_Stop();
        }
        else if (cmd.motor_enable)
        {
            pwm_out = (uint16_t)((uint32_t)cmd.pwm_target_pct
                                 * (uint32_t)MOTOR_PWM_MAX / 100U);
            Motor_SetPWM(pwm_out, cmd.direction_fwd);
        }
        else
        {
            Motor_Stop();
        }

        /* --- 5. Ghi toàn bộ motor data vào g_ecu_data — 1 lần acquire ---
         * Gộp motor_pwm + speed + odometer vào cùng 1 critical section
         * để CAN Transmit đọc được snapshot nhất quán */
        (void)osMutexAcquire(xEcuMutex, osWaitForever);
        g_ecu_data.motor_pwm  = pwm_out;
        g_ecu_data.speed_kmh  = speed_kmh;
        g_ecu_data.odometer_m += dist_mm / 1000U;
        (void)osMutexRelease(xEcuMutex);

        tick += TASK_PERIOD_MOTOR_MS;
        osDelayUntil(tick);
    }
}
