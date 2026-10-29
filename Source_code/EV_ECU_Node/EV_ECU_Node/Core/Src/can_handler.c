/**
 * @file    can_handler.c
 * @brief   CAN TX: gửi EcuData lên VCU | CAN RX: nhận VcuCommand từ VCU
 *
 * CAN TX schedule (base period 20ms):
 *   0x100 — Speed + Accel       : mỗi 20ms
 *   0x101 — Battery             : mỗi 100ms
 *   0x102 — Hardware inputs     : mỗi 40ms
 *   0x103 — Odometer            : mỗi 40ms
 *
 * CAN RX:
 *   0x200 — VcuCommand từ Pi    : event-driven qua interrupt
 *
 * @note  MISRA C:2012 compliant (basic rules)
 */

#include "can_handler.h"
#include "ecu_data.h"
#include "app_config.h"
#include "vehicle_types.h"
#include <string.h>
#include <stdio.h>

/* =========================================================
 * SECTION 1: PRIVATE TYPES & VARIABLES
 * ========================================================= */

/**
 * @brief Raw CAN frame — truyền từ ISR vào Task_CANReceive qua queue
 */
typedef struct
{
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
} CanRawFrame_t;

/** Queue CAN RX — ISR push (timeout=0), Task_CANReceive pop (block) */
static osMessageQueueId_t s_can_rx_queue = NULL;

/** Counters điều khiển lịch gửi TX */
static uint8_t s_cnt_100ms = 0U;
static uint8_t s_cnt_40ms  = 0U;

/* =========================================================
 * SECTION 2: PRIVATE — CAN TX HELPERS
 * ========================================================= */

/**
 * @brief Gửi 1 CAN frame qua HAL
 */
static HAL_StatusTypeDef CAN_SendFrame(uint32_t  id,
                                       uint8_t  *data,
                                       uint8_t   len)
{
    CAN_TxHeaderTypeDef hdr;
    uint32_t            mailbox;

    hdr.StdId              = id;
    hdr.ExtId              = 0U;
    hdr.IDE                = CAN_ID_STD;
    hdr.RTR                = CAN_RTR_DATA;
    hdr.DLC                = len;
    hdr.TransmitGlobalTime = DISABLE;

    return HAL_CAN_AddTxMessage(&CAN_HANDLE, &hdr, data, &mailbox);
}

/**
 * @brief 0x100 — Speed + Accel (mỗi 20ms)
 */
static void CAN_Send_0x100(const EcuData_t *e)
{
    CanMsg100_t msg;
    msg.speed_x10     = (uint16_t)(e->speed_kmh * 10.0f);
    msg.accel_pct     = e->accel_pct;
    msg.motor_pwm_pct = (uint8_t)((e->motor_pwm * 100U) / MOTOR_PWM_MAX);
    (void)CAN_SendFrame(0x100U, (uint8_t *)&msg, sizeof(CanMsg100_t));
}

/**
 * @brief 0x101 — Battery (mỗi 100ms)
 */
static void CAN_Send_0x101(const EcuData_t *e)
{
    CanMsg101_t msg;
    msg.batt_mv    = (uint16_t)(e->batt_voltage * 1000.0f);
    msg.soc_pct    = e->soc_pct;
    msg.batt_flags = e->batt_low_warning ? 0x01U : 0x00U;
    (void)CAN_SendFrame(0x101U, (uint8_t *)&msg, sizeof(CanMsg101_t));
}

/**
 * @brief 0x102 — Hardware inputs (mỗi 40ms)
 *        VCU cần thông tin này để chạy state machine
 */
static void CAN_Send_0x102(const EcuData_t *e)
{
    CanMsg102_t msg;
    msg.gear       = (uint8_t)e->gear;
    msg.drive_mode = (uint8_t)e->drive_mode;
    msg.reserved   = 0U;

    /* Pack 4 boolean flags vào 1 byte */
    msg.input_flags = 0U;
    if (e->key_on)          { msg.input_flags |= 0x01U; }
    if (e->brake_pressed)   { msg.input_flags |= 0x02U; }
    if (e->charging_detect) { msg.input_flags |= 0x04U; }
    if (e->fault_inject)    { msg.input_flags |= 0x08U; }

    (void)CAN_SendFrame(0x102U, (uint8_t *)&msg, sizeof(CanMsg102_t));
}

/**
 * @brief 0x103 — Odometer (mỗi 40ms)
 */
static void CAN_Send_0x103(const EcuData_t *e)
{
    CanMsg103_t msg;
    msg.odometer_m = (uint16_t)(e->odometer_m & 0xFFFFU);
    msg.reserved0  = 0U;
    msg.reserved1  = 0U;
    (void)CAN_SendFrame(0x103U, (uint8_t *)&msg, sizeof(CanMsg103_t));
}

/* =========================================================
 * SECTION 3: CAN RX — ISR CALLBACK
 * ========================================================= */

/**
 * @brief HAL Callback — gọi từ ISR khi có frame vào FIFO0
 *
 * @note  KHÔNG block, KHÔNG gọi printf trong ISR.
 *        Chỉ đọc frame và push vào queue với timeout = 0.
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_cb)
{
    CAN_RxHeaderTypeDef hdr;
    CanRawFrame_t       frame;

    (void)memset(&frame, 0, sizeof(CanRawFrame_t));

    if (HAL_CAN_GetRxMessage(hcan_cb, CAN_RX_FIFO0,
                             &hdr, frame.data) == HAL_OK)
    {
        frame.id  = hdr.StdId;
        frame.dlc = (uint8_t)hdr.DLC;

        if (s_can_rx_queue != NULL)
        {
            /* timeout = 0 — không block trong ISR */
            (void)osMessageQueuePut(s_can_rx_queue, &frame, 0U, 0U);
        }
    }
}

/* =========================================================
 * SECTION 4: PUBLIC FUNCTIONS
 * ========================================================= */

/**
 * @brief Khởi tạo CAN Handler
 *        CAN đã được Start trong main() trước osKernelStart.
 *        Ở đây chỉ cấu hình filter, notification và tạo RX queue.
 */
void CANHandler_Init(void)
{
    /* Filter — chỉ nhận ID 0x200 từ VCU */
    CAN_FilterTypeDef filter;
    filter.FilterBank           = 0U;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = (0x200U << 5U);
    filter.FilterIdLow          = 0x0000U;
    filter.FilterMaskIdHigh     = (0x7FFU << 5U);  /* Exact match 0x200 */
    filter.FilterMaskIdLow      = 0x0000U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    (void)HAL_CAN_ConfigFilter(&CAN_HANDLE, &filter);

    /* Bật interrupt nhận FIFO0 */
    (void)HAL_CAN_ActivateNotification(&CAN_HANDLE,
                                        CAN_IT_RX_FIFO0_MSG_PENDING);

    /* Tạo RX queue — capacity 8 frames */
    s_can_rx_queue = osMessageQueueNew(8U, sizeof(CanRawFrame_t), NULL);

    printf("[CAN] Init OK\r\n");
}

/**
 * @brief Task CAN Transmit — base period 20ms
 *
 * Snapshot g_ecu_data → giữ mutex ngắn nhất
 * → gửi các frame theo lịch định sẵn
 */
void Task_CANTransmit(void *argument)
{
    uint32_t tick = osKernelGetTickCount();

    for (;;)
    {
        /* Snapshot toàn bộ EcuData — giữ mutex ngắn nhất có thể */
        EcuData_t snap;
        (void)osMutexAcquire(xEcuMutex, osWaitForever);
        snap = g_ecu_data;
        (void)osMutexRelease(xEcuMutex);

        /* 0x100 — mỗi 20ms (mỗi lần task chạy) */
        CAN_Send_0x100(&snap);

        /* 0x102, 0x103 — mỗi 40ms (mỗi 2 lần task chạy) */
        s_cnt_40ms++;
        if (s_cnt_40ms >= 2U)
        {
            s_cnt_40ms = 0U;
            CAN_Send_0x102(&snap);
            CAN_Send_0x103(&snap);
        }

        /* 0x101 — mỗi 100ms (mỗi 5 lần task chạy) */
        s_cnt_100ms++;
        if (s_cnt_100ms >= 5U)
        {
            s_cnt_100ms = 0U;
            CAN_Send_0x101(&snap);
        }

        tick += TASK_PERIOD_CAN_MS;
        osDelayUntil(tick);
    }
}

/**
 * @brief Task CAN Receive — event-driven, block chờ frame từ ISR
 *
 * Luồng:
 *   Block chờ s_can_rx_queue (ISR push vào)
 *   → validate ID và DLC
 *   → parse CanMsg200_t → VcuCommand_t
 *   → ghi vào g_vcu_cmd + cập nhật g_vcu_last_tick (watchdog)
 *   → chỉ log khi command thay đổi (tránh flood UART)
 */
void Task_CANReceive(void *argument)
{
    CanRawFrame_t frame;
    VcuCommand_t  last_cmd;

    (void)memset(&last_cmd, 0, sizeof(VcuCommand_t));

    for (;;)
    {
        /* Block vô thời hạn — tiết kiệm CPU, không polling */
        if (osMessageQueueGet(s_can_rx_queue, &frame,
                              NULL, osWaitForever) == osOK)
        {
            /* Chỉ xử lý ID 0x200 với đủ 4 bytes */
            if ((frame.id == 0x200U) && (frame.dlc >= 4U))
            {
                /* Parse CanMsg200_t */
                VcuCommand_t cmd;
                cmd.motor_enable   = (frame.data[0] & 0x01U) != 0U;
                cmd.direction_fwd  = (frame.data[0] & 0x02U) != 0U;
                cmd.pwm_target_pct = frame.data[1];
                cmd.vehicle_state  = frame.data[2];
                cmd.fault_flags    = frame.data[3];

                /* Clamp pwm_target về 100% */
                if (cmd.pwm_target_pct > 100U)
                {
                    cmd.pwm_target_pct = 100U;
                }

                /* Ghi vào g_vcu_cmd + cập nhật watchdog timestamp */
                (void)osMutexAcquire(xVcuCmdMutex, osWaitForever);
                g_vcu_cmd       = cmd;
                g_vcu_last_tick = osKernelGetTickCount();
                (void)osMutexRelease(xVcuCmdMutex);

                /* Log chỉ khi có thay đổi — tránh flood UART */
                if (memcmp(&cmd, &last_cmd, sizeof(VcuCommand_t)) != 0)
                {
                    printf("[VCU] enable=%d fwd=%d pwm=%d%% state=%d\r\n",
                           (int)cmd.motor_enable,
                           (int)cmd.direction_fwd,
                           (int)cmd.pwm_target_pct,
                           (int)cmd.vehicle_state);
                    last_cmd = cmd;
                }
            }
        }
    }
}
