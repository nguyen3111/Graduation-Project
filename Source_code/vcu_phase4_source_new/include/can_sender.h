/**
 * @file    can_sender.h
 * @brief   CAN Sender — gửi VcuCommand (0x200) xuống STM32 mỗi 20ms
 *
 * CanSender chạy trong thread riêng với chu kỳ cố định 20ms.
 * Mỗi cycle: đọc VcuCommand từ VehicleData → pack vào CanMsg200_t → write().
 *
 * Thread-safety:
 *   VehicleData::getVcuCommand() đã có mutex nội bộ — CanSender không cần lock thêm.
 */

#ifndef CAN_SENDER_H
#define CAN_SENDER_H

#include "vehicle_types_pi.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

class CanSender
{
public:
    explicit CanSender(const std::string& interface_name = "can0");
    ~CanSender();

    bool start();
    void stop();
    bool isRunning() const;
    uint32_t getSentCount() const;

private:
    void threadFunc();
    bool sendVcuCommand();

    std::string           m_interface;
    int                   m_socket_fd {-1};
    std::thread           m_thread;
    std::atomic<bool>     m_running    {false};
    std::atomic<uint32_t> m_sent_count {0U};

    static constexpr uint32_t TX_PERIOD_MS = 20U;
};

#endif /* CAN_SENDER_H */
