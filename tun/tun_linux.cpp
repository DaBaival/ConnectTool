#include "tun_linux.h"

#ifdef __linux__

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <errno.h>

namespace tun {

TunLinux::TunLinux() 
    : fd_(-1)
    , mtu_(1500) {
}

TunLinux::~TunLinux() {
    close();
}

bool TunLinux::open(const std::string& device_name, uint32_t mtu) {
    if (is_open()) {
        last_error_ = "TUN device already open";
        return false;
    }

    // 打开 TUN 设备
    fd_ = ::open("/dev/net/tun", O_RDWR);
    if (fd_ < 0) {
        last_error_ = std::string("Failed to open /dev/net/tun: ") + strerror(errno);
        return false;
    }

    // 配置 TUN 设备
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    
    // IFF_TUN: TUN 设备（三层，IP 包）
    // IFF_NO_PI: 不包含额外的包信息头
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    
    if (!device_name.empty()) {
        strncpy(ifr.ifr_name, device_name.c_str(), IFNAMSIZ - 1);
    }

    // 创建 TUN 设备
    if (ioctl(fd_, TUNSETIFF, &ifr) < 0) {
        last_error_ = std::string("ioctl TUNSETIFF failed: ") + strerror(errno);
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    device_name_ = ifr.ifr_name;
    mtu_ = mtu;

    return true;
}

void TunLinux::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    device_name_.clear();
}

bool TunLinux::is_open() const {
    return fd_ >= 0;
}

std::string TunLinux::get_device_name() const {
    return device_name_;
}

bool TunLinux::set_ip(const std::string& ip_address, const std::string& netmask) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return false;
    }

    // 创建一个临时 socket 用于 ioctl 操作
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        last_error_ = std::string("Failed to create socket: ") + strerror(errno);
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device_name_.c_str(), IFNAMSIZ - 1);

    // 设置 IP 地址
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_address.c_str(), &addr->sin_addr) != 1) {
        last_error_ = "Invalid IP address format";
        ::close(sock);
        return false;
    }

    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
        last_error_ = std::string("Failed to set IP address: ") + strerror(errno);
        ::close(sock);
        return false;
    }

    // 设置子网掩码
    struct sockaddr_in* mask = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask);
    mask->sin_family = AF_INET;
    if (inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr) != 1) {
        last_error_ = "Invalid netmask format";
        ::close(sock);
        return false;
    }

    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
        last_error_ = std::string("Failed to set netmask: ") + strerror(errno);
        ::close(sock);
        return false;
    }

    // 设置 MTU
    ifr.ifr_mtu = mtu_;
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        last_error_ = std::string("Failed to set MTU: ") + strerror(errno);
        ::close(sock);
        return false;
    }

    ::close(sock);
    return true;
}

bool TunLinux::set_up() {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return false;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        last_error_ = std::string("Failed to create socket: ") + strerror(errno);
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device_name_.c_str(), IFNAMSIZ - 1);

    // 获取当前标志
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        last_error_ = std::string("Failed to get interface flags: ") + strerror(errno);
        ::close(sock);
        return false;
    }

    // 设置 UP 和 RUNNING 标志
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        last_error_ = std::string("Failed to set interface UP: ") + strerror(errno);
        ::close(sock);
        return false;
    }

    ::close(sock);
    return true;
}

int TunLinux::read(uint8_t* buffer, size_t max_length) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return -1;
    }

    ssize_t n = ::read(fd_, buffer, max_length);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // 非阻塞模式下没有数据
        }
        last_error_ = std::string("Read failed: ") + strerror(errno);
        return -1;
    }

    return static_cast<int>(n);
}

int TunLinux::write(const uint8_t* buffer, size_t length) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return -1;
    }

    ssize_t n = ::write(fd_, buffer, length);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // 非阻塞模式下无法写入
        }
        last_error_ = std::string("Write failed: ") + strerror(errno);
        return -1;
    }

    return static_cast<int>(n);
}

std::string TunLinux::get_last_error() const {
    return last_error_;
}

uint32_t TunLinux::get_mtu() const {
    return mtu_;
}

bool TunLinux::set_non_blocking(bool non_blocking) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return false;
    }

    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        last_error_ = std::string("fcntl F_GETFL failed: ") + strerror(errno);
        return false;
    }

    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd_, F_SETFL, flags) < 0) {
        last_error_ = std::string("fcntl F_SETFL failed: ") + strerror(errno);
        return false;
    }

    return true;
}



uint32_t TunLinux::get_interface_index() const {
    if (!is_open() || device_name_.empty()) {
        return 0;
    }
    return if_nametoindex(device_name_.c_str());
}

} // namespace tun

#endif // __linux__
