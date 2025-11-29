#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tun {

/**
 * @brief 抽象的 TUN 设备接口
 * 
 * 这个接口定义了跨平台的 TUN 设备操作。
 * 具体实现由平台特定的子类提供：
 * - Linux: TunLinux (使用 /dev/net/tun)
 * - macOS: TunMacOS (使用 utun)
 * - Windows: TunWindows (使用 Wintun)
 */
class TunInterface {
public:
    virtual ~TunInterface() = default;

    /**
     * @brief 打开/创建 TUN 设备
     * @param device_name 设备名称（可选，某些平台会自动分配）
     * @param mtu 最大传输单元（默认 1500）
     * @return true 成功，false 失败
     */
    virtual bool open(const std::string& device_name = "", uint32_t mtu = 1500) = 0;

    /**
     * @brief 关闭 TUN 设备
     */
    virtual void close() = 0;

    /**
     * @brief 检查设备是否已打开
     */
    virtual bool is_open() const = 0;

    /**
     * @brief 获取实际的设备名称
     * @return 设备名称（如 "tun0", "utun3", "wintun"）
     */
    virtual std::string get_device_name() const = 0;

    /**
     * @brief 设置 IP 地址和子网掩码
     * @param ip_address IP 地址（如 "10.0.0.1"）
     * @param netmask 子网掩码（如 "255.255.255.0"）
     * @return true 成功，false 失败
     */
    virtual bool set_ip(const std::string& ip_address, const std::string& netmask) = 0;

    /**
     * @brief 启用设备（设置为 UP 状态）
     * @return true 成功，false 失败
     */
    virtual bool set_up() = 0;

    /**
     * @brief 从 TUN 设备读取数据包
     * @param buffer 缓冲区
     * @param max_length 最大读取长度
     * @return 实际读取的字节数，-1 表示错误
     */
    virtual int read(uint8_t* buffer, size_t max_length) = 0;

    /**
     * @brief 向 TUN 设备写入数据包
     * @param buffer 数据缓冲区
     * @param length 数据长度
     * @return 实际写入的字节数，-1 表示错误
     */
    virtual int write(const uint8_t* buffer, size_t length) = 0;

    /**
     * @brief 获取最后的错误信息
     */
    virtual std::string get_last_error() const = 0;

    /**
     * @brief 获取设备的 MTU
     */
    virtual uint32_t get_mtu() const = 0;

    /**
     * @brief 设置非阻塞模式
     * @param non_blocking true 为非阻塞，false 为阻塞
     * @return true 成功，false 失败
     */
    virtual bool set_non_blocking(bool non_blocking) = 0;

    /**
     * @brief Get the interface index (useful for system commands like route)
     * @return Interface index (0 if not supported or invalid)
     */
    virtual uint32_t get_interface_index() const { return 0; }
};

/**
 * @brief 工厂函数：创建当前平台的 TUN 实例
 * @return 平台特定的 TUN 实现
 */
std::unique_ptr<TunInterface> create_tun();

} // namespace tun
