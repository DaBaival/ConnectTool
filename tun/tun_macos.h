#pragma once

#include "tun_interface.h"

#ifdef __APPLE__

namespace tun {

/**
 * @brief macOS 平台的 TUN 实现
 * 
 * 使用 utun 内核控制接口（System Sockets）
 */
class TunMacOS : public TunInterface {
public:
    TunMacOS();
    ~TunMacOS() override;

    bool open(const std::string& device_name = "", uint32_t mtu = 1500) override;
    void close() override;
    bool is_open() const override;
    std::string get_device_name() const override;
    bool set_ip(const std::string& ip_address, const std::string& netmask) override;
    bool set_up() override;
    int read(uint8_t* buffer, size_t max_length) override;
    int write(const uint8_t* buffer, size_t length) override;
    std::string get_last_error() const override;
    uint32_t get_mtu() const override;
    bool set_non_blocking(bool non_blocking) override;
    uint32_t get_interface_index() const override;

private:
    int fd_;                    // utun 设备文件描述符
    std::string device_name_;   // 设备名称
    uint32_t mtu_;              // MTU
    std::string last_error_;    // 最后的错误信息
    int unit_number_;           // utun 单元号
};

} // namespace tun

#endif // __APPLE__
