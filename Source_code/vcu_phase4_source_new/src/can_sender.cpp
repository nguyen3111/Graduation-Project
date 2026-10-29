/**
 * @file    can_sender.cpp
 * @brief   CanSender implementation — gửi 0x200 mỗi 20ms
 */

#include "can_sender.h"
#include "vehicle_data.h"

#ifdef PLATFORM_LINUX
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

#include <chrono>
#include <thread>
#include <cstdio>

CanSender::CanSender(const std::string& interface_name)
    : m_interface(interface_name)
{
}

CanSender::~CanSender()
{
    stop();
}

bool CanSender::start()
{
#ifndef PLATFORM_LINUX
    fprintf(stderr, "[CanSender] SocketCAN chỉ hoạt động trên Linux\n");
    return false;
#else
    m_socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket_fd < 0)
    {
        fprintf(stderr, "[CanSender] socket() failed: %s\n", strerror(errno));
        return false;
    }

    struct ifreq ifr;
    (void)memset(&ifr, 0, sizeof(ifr));
    (void)strncpy(ifr.ifr_name, m_interface.c_str(), IFNAMSIZ - 1U);
    if (ioctl(m_socket_fd, SIOCGIFINDEX, &ifr) < 0)
    {
        fprintf(stderr, "[CanSender] ioctl failed: %s\n", strerror(errno));
        (void)close(m_socket_fd);
        m_socket_fd = -1;
        return false;
    }

    struct sockaddr_can addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(m_socket_fd,
             reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) < 0)
    {
        fprintf(stderr, "[CanSender] bind() failed: %s\n", strerror(errno));
        (void)close(m_socket_fd);
        m_socket_fd = -1;
        return false;
    }

    m_running = true;
    m_thread  = std::thread(&CanSender::threadFunc, this);

    fprintf(stdout, "[CanSender] Started on %s\n", m_interface.c_str());
    return true;
#endif
}

void CanSender::stop()
{
    m_running = false;

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

    fprintf(stdout, "[CanSender] Stopped. Total sent: %u\n",
            m_sent_count.load());
}

bool CanSender::isRunning() const { return m_running.load(); }
uint32_t CanSender::getSentCount() const { return m_sent_count.load(); }

void CanSender::threadFunc()
{
    auto next_tick = std::chrono::steady_clock::now();

    while (m_running.load())
    {
        (void)sendVcuCommand();
        next_tick += std::chrono::milliseconds(TX_PERIOD_MS);
        std::this_thread::sleep_until(next_tick);
    }
}

bool CanSender::sendVcuCommand()
{
#ifndef PLATFORM_LINUX
    return false;
#else
    VcuCommand cmd = VehicleData::instance().getVcuCommand();

    CanMsg200_t msg;
    msg.ctrl_flags     = 0U;
    if (cmd.motor_enable)  { msg.ctrl_flags |= 0x01U; }
    if (cmd.direction_fwd) { msg.ctrl_flags |= 0x02U; }
    msg.pwm_target_pct = cmd.pwm_target_pct;
    /* FIX: cast VehicleState enum class → uint8_t trước khi gán vào struct */
    msg.vehicle_state  = static_cast<uint8_t>(cmd.vehicle_state);
    msg.fault_flags    = cmd.fault_flags;

    struct can_frame frame;
    (void)memset(&frame, 0, sizeof(frame));
    frame.can_id  = CAN_ID_VCU_COMMAND;
    frame.can_dlc = sizeof(CanMsg200_t);
    (void)memcpy(frame.data, &msg, sizeof(msg));

    ssize_t nbytes = write(m_socket_fd, &frame, sizeof(frame));
    if (nbytes != static_cast<ssize_t>(sizeof(frame)))
    {
        fprintf(stderr, "[CanSender] write() failed: %s\n", strerror(errno));
        return false;
    }

    m_sent_count.fetch_add(1U);

    fprintf(stdout,
            "[CAN TX] 0x200: enable=%u, fwd=%u, pwm=%u%%, state=%u, fault=0x%02X\n",
            static_cast<unsigned>(cmd.motor_enable),
            static_cast<unsigned>(cmd.direction_fwd),
            cmd.pwm_target_pct,
            /* FIX: cast enum class → unsigned cho printf */
            static_cast<unsigned>(cmd.vehicle_state),
            cmd.fault_flags);

    return true;
#endif
}
