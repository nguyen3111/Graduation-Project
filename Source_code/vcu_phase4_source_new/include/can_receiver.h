/**
 * @file    can_receiver.h
 * @brief   CAN Receiver — đọc frame từ SocketCAN, parse và ghi vào VehicleData
 *
 * CanReceiver chạy trong thread riêng, block trên read() chờ frame mới
 * từ kernel SocketCAN driver (can0). Khi có frame, nó parse theo CAN ID
 * rồi gọi VehicleData::updateXxx() tương ứng.
 *
 * Thread model:
 *   - Thread: m_thread (std::thread)
 *   - Dừng: gọi stop() → set m_running = false → thread thoát vòng lặp
 *   - Socket: AF_CAN raw socket, blocking read với timeout SO_RCVTIMEO
 *
 * Không cần Qt — đây là pure C++ thread.
 */

#ifndef CAN_RECEIVER_H
#define CAN_RECEIVER_H

#include "vehicle_types_pi.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>
#include <functional>

class CanReceiver
{
public:
    /**
     * @brief Callback type — gọi mỗi khi nhận được frame hợp lệ
     *        VcuBridge dùng để emit Qt signal sang QML
     */
    using FrameCallback = std::function<void()>;

    /**
     * @brief Constructor
     * @param interface_name  Tên SocketCAN interface, mặc định "can0"
     */
    explicit CanReceiver(const std::string& interface_name = "can0");

    ~CanReceiver();

    /**
     * @brief Mở socket, khởi động receive thread
     * @return true nếu thành công
     */
    bool start();

    /**
     * @brief Yêu cầu thread dừng và join
     */
    void stop();

    /**
     * @brief Đăng ký callback — gọi sau mỗi frame được parse
     *        VcuBridge đăng ký để emit signal sang QML
     */
    void setOnFrameReceived(FrameCallback cb);

    /**
     * @brief Kiểm tra socket đã mở chưa
     */
    bool isRunning() const;

    /**
     * @brief Số frame đã nhận — dùng để debug / stats
     */
    uint32_t getFrameCount() const;

private:
    /**
     * @brief Entry point của receive thread
     */
    void threadFunc();

    /**
     * @brief Parse frame theo CAN ID và cập nhật VehicleData
     * @param can_id   Standard 11-bit ID
     * @param data     Pointer tới 8 bytes dữ liệu
     * @param dlc      Data Length Code
     */
    void parseFrame(uint32_t can_id, const uint8_t* data, uint8_t dlc);

    /* Tên interface SocketCAN (vd: "can0") */
    std::string m_interface;

    /* File descriptor của AF_CAN socket */
    int m_socket_fd {-1};

    /* Thread control */
    std::thread       m_thread;
    std::atomic<bool> m_running {false};

    /* Callback gọi sau mỗi frame parse thành công */
    FrameCallback m_on_frame_rx;

    /* Statistics */
    std::atomic<uint32_t> m_frame_count {0U};
};

#endif /* CAN_RECEIVER_H */
