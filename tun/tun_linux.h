#pragma once

#include "tun_interface.h"

#ifdef __linux__

namespace tun {

/**
 * @brief Linux 平台的 TUN 实现
 * 
 * 使用 /dev/net/tun 字符设备和 ioctl 系统调用
 */
class TunLinux : public TunInterface {
public:
    TunLinux();
    ~TunLinux() override;

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
    int fd_;                    // TUN 设备文件描述符
    std::string device_name_;   // 设备名称
    uint32_t mtu_;              // MTU
    std::string last_error_;    // 最后的错误信息
};

} // namespace tun

#endif // __linux__
