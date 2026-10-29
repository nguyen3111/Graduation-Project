/**
 * @file    can_receiver.cpp
 * @brief   SocketCAN CAN Receiver implementation
 *
 * Luồng hoạt động:
 *   1. start()    → socket(AF_CAN) → bind(can0) → thread bắt đầu
 *   2. threadFunc → vòng lặp: read(socket) [blocking, 100ms timeout]
 *   3. Mỗi frame  → parseFrame() → VehicleData::updateXxx()
 *   4. stop()     → m_running=false → thread exit → join
 *
 * Timeout 100ms trên SO_RCVTIMEO cho phép thread kiểm tra m_running
 * định kỳ mà không block vĩnh viễn — đây là pattern chuẩn cho
 * graceful shutdown của I/O thread.
 */

#include "can_receiver.h"
#include "vehicle_data.h"

/* SocketCAN headers — chỉ có trên Linux */
#ifdef PLATFORM_LINUX
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

#include <cstdio>
#include <stdexcept>

/* ============================================================
 * Constructor / Destructor
 * ============================================================ */

CanReceiver::CanReceiver(const std::string& interface_name)
    : m_interface(interface_name)
{
}

CanReceiver::~CanReceiver()
{
    stop();
}

/* ============================================================
 * Public: start()
 * ============================================================ */

bool CanReceiver::start()
{
#ifndef PLATFORM_LINUX
    /* Stub cho môi trường non-Linux (desktop testing) */
    fprintf(stderr, "[CanReceiver] SocketCAN chỉ hoạt động trên Linux\n");
    return false;
#else
    /* ---- 1. Tạo raw CAN socket ---- */
    m_socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket_fd < 0)
    {
        fprintf(stderr, "[CanReceiver] socket() failed: %s\n", strerror(errno));
        return false;
    }

    /* ---- 2. Resolve tên interface sang index ---- */
    struct ifreq ifr;
    (void)memset(&ifr, 0, sizeof(ifr));
    /* strncpy an toàn — IFNAMSIZ là 16 byte */
    (void)strncpy(ifr.ifr_name, m_interface.c_str(), IFNAMSIZ - 1U);
    if (ioctl(m_socket_fd, SIOCGIFINDEX, &ifr) < 0)
    {
        fprintf(stderr, "[CanReceiver] ioctl SIOCGIFINDEX failed: %s\n",
                strerror(errno));
        (void)close(m_socket_fd);
        m_socket_fd = -1;
        return false;
    }

    /* ---- 3. Bind socket vào interface can0 ---- */
    struct sockaddr_can addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(m_socket_fd,
             reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) < 0)
    {
        fprintf(stderr, "[CanReceiver] bind() failed: %s\n", strerror(errno));
        (void)close(m_socket_fd);
        m_socket_fd = -1;
        return false;
    }

    /* ---- 4. Set receive timeout 100ms ---- */
    /* Cho phép thread kiểm tra m_running mỗi 100ms */
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 100000;  /* 100ms */
    if (setsockopt(m_socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv)) < 0)
    {
        fprintf(stderr, "[CanReceiver] setsockopt SO_RCVTIMEO failed: %s\n",
                strerror(errno));
        /* Không fatal — vẫn tiếp tục, chỉ không có timeout */
    }

    /* ---- 5. Chỉ nhận frame từ STM32: filter ID 0x100–0x103 ---- */
    /* 4 filter rules — chỉ nhận đúng các ID cần thiết */
    struct can_filter filters[4];
    filters[0].can_id   = CAN_ID_SPEED_ACCEL;
    filters[0].can_mask = CAN_SFF_MASK;  /* Match chính xác 11-bit ID */
    filters[1].can_id   = CAN_ID_BATTERY;
    filters[1].can_mask = CAN_SFF_MASK;
    filters[2].can_id   = CAN_ID_HW_INPUTS;
    filters[2].can_mask = CAN_SFF_MASK;
    filters[3].can_id   = CAN_ID_ODOMETER;
    filters[3].can_mask = CAN_SFF_MASK;

    if (setsockopt(m_socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                   filters, sizeof(filters)) < 0)
    {
        fprintf(stderr, "[CanReceiver] setsockopt CAN_RAW_FILTER failed: %s\n",
                strerror(errno));
        /* Không fatal — vẫn nhận mọi frame, chỉ mất tối ưu filter */
    }

    /* ---- 6. Khởi động receive thread ---- */
    m_running = true;
    m_thread  = std::thread(&CanReceiver::threadFunc, this);

    fprintf(stdout, "[CanReceiver] Started on %s\n", m_interface.c_str());
    return true;
#endif /* PLATFORM_LINUX */
}

/* ============================================================
 * Public: stop()
 * ============================================================ */

void CanReceiver::stop()
{
    m_running = false;

    /* Đóng socket → buộc read() trả về ngay nếu đang block */
#ifdef PLATFORM_LINUX
    if (m_socket_fd >= 0)
    {
        (void)close(m_socket_fd);
        m_socket_fd = -1;
    }
#endif

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    fprintf(stdout, "[CanReceiver] Stopped. Total frames: %u\n",
            m_frame_count.load());
}

void CanReceiver::setOnFrameReceived(FrameCallback cb)
{
    m_on_frame_rx = std::move(cb);
}

bool CanReceiver::isRunning() const
{
    return m_running.load();
}

uint32_t CanReceiver::getFrameCount() const
{
    return m_frame_count.load();
}

/* ============================================================
 * Private: threadFunc() — vòng lặp nhận CAN frame
 * ============================================================ */

void CanReceiver::threadFunc()
{
#ifdef PLATFORM_LINUX
    struct can_frame frame;

    while (m_running.load())
    {
        /* ---- Blocking read với timeout 100ms ---- */
        ssize_t nbytes = read(m_socket_fd, &frame, sizeof(frame));

        if (nbytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Timeout bình thường — kiểm tra m_running rồi loop lại */
                continue;
            }
            if (errno == EBADF)
            {
                /* Socket bị đóng bởi stop() — thoát bình thường */
                break;
            }
            fprintf(stderr, "[CanReceiver] read() error: %s\n", strerror(errno));
            continue;
        }

        if (static_cast<size_t>(nbytes) < sizeof(struct can_frame))
        {
            /* Frame không đủ dài — bỏ qua */
            continue;
        }

        /* ---- Parse frame ---- */
        uint32_t can_id = frame.can_id & CAN_SFF_MASK;  /* Lấy 11-bit ID */
        uint8_t  dlc    = frame.can_dlc;

        parseFrame(can_id, frame.data, dlc);

        /* ---- Gọi callback để thông báo cho VcuBridge ---- */
        if (m_on_frame_rx)
        {
            m_on_frame_rx();
        }

        m_frame_count.fetch_add(1U);
    }
#endif /* PLATFORM_LINUX */

    fprintf(stdout, "[CanReceiver] Thread exited.\n");
}

/* ============================================================
 * Private: parseFrame() — dispatch theo CAN ID
 * ============================================================ */

void CanReceiver::parseFrame(uint32_t        can_id,
                              const uint8_t*  data,
                              uint8_t         dlc)
{
    VehicleData& vd = VehicleData::instance();

    switch (can_id)
    {
        /* ---- 0x100 — Speed + Accel (DLC = 4) ---- */
        case CAN_ID_SPEED_ACCEL:
        {
            if (dlc < 4U) { break; }

            CanMsg100_t msg;
            (void)memcpy(&msg, data, sizeof(msg));

            vd.updateSpeedAccel(msg.speed_x10, msg.accel_pct, msg.motor_pwm_pct);

            fprintf(stdout, "[CAN RX] 0x100: speed=%.1f km/h, accel=%u%%, pwm=%u%%\n",
                    static_cast<double>(msg.speed_x10) / 10.0,
                    msg.accel_pct,
                    msg.motor_pwm_pct);
            break;
        }

        /* ---- 0x101 — Battery (DLC = 4) ---- */
        case CAN_ID_BATTERY:
        {
            if (dlc < 4U) { break; }

            CanMsg101_t msg;
            (void)memcpy(&msg, data, sizeof(msg));

            vd.updateBattery(msg.batt_mv, msg.soc_pct, msg.batt_flags);

            fprintf(stdout, "[CAN RX] 0x101: batt=%.3fV, soc=%u%%, flags=0x%02X\n",
                    static_cast<double>(msg.batt_mv) / 1000.0,
                    msg.soc_pct,
                    msg.batt_flags);
            break;
        }

        /* ---- 0x102 — Hardware Inputs (DLC = 4) ---- */
        case CAN_ID_HW_INPUTS:
        {
            if (dlc < 4U) { break; }

            CanMsg102_t msg;
            (void)memcpy(&msg, data, sizeof(msg));

            vd.updateHwInputs(msg.gear, msg.drive_mode, msg.input_flags);

            fprintf(stdout, "[CAN RX] 0x102: gear=%u, mode=%u, flags=0x%02X\n",
                    msg.gear,
                    msg.drive_mode,
                    msg.input_flags);
            break;
        }

        /* ---- 0x103 — Odometer (DLC = 4) ---- */
        case CAN_ID_ODOMETER:
        {
            if (dlc < 4U) { break; }

            CanMsg103_t msg;
            (void)memcpy(&msg, data, sizeof(msg));

            vd.updateOdometer(msg.odometer_m);

            fprintf(stdout, "[CAN RX] 0x103: odometer=%u m\n", msg.odometer_m);
            break;
        }

        default:
            /* CAN ID không thuộc danh sách — bỏ qua */
            fprintf(stdout, "[CAN RX] Unknown ID: 0x%03X, DLC=%u\n", can_id, dlc);
            break;
    }
}
