#include "tun_windows.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <cstring>
#include <sstream>
#include <codecvt>
#include <locale>
#include <objbase.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define WINTUN_MIN_RING_CAPACITY 0x20000  // 128 KiB
#define WINTUN_MAX_RING_CAPACITY 0x4000000  // 64 MiB

namespace tun {

TunWindows::TunWindows() 
    : adapter_(nullptr)
    , session_(nullptr)
    , mtu_(1500)
    , non_blocking_(false)
    , read_event_(nullptr)
    , wintun_dll_(nullptr)
    , WintunCreateAdapter_(nullptr)
    , WintunOpenAdapter_(nullptr)
    , WintunCloseAdapter_(nullptr)
    , WintunStartSession_(nullptr)
    , WintunEndSession_(nullptr)
    , WintunGetReadWaitEvent_(nullptr)
    , WintunReceivePacket_(nullptr)
    , WintunReleaseReceivePacket_(nullptr)
    , WintunAllocateSendPacket_(nullptr)
    , WintunSendPacket_(nullptr)
    , WintunGetAdapterLUID_(nullptr) {
    // 生成随机 GUID
    CoCreateGuid(&adapter_guid_);
}

TunWindows::~TunWindows() {
    close();
}

std::wstring TunWindows::string_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::string TunWindows::get_windows_error(DWORD error_code) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

bool TunWindows::load_wintun_dll() {
    // 尝试从多个位置加载 wintun.dll
    const char* dll_paths[] = {
        "wintun.dll",
        "third_party/wintun/bin/amd64/wintun.dll",
        "third_party/wintun/bin/x86/wintun.dll",
        "third_party/wintun/bin/arm64/wintun.dll"
    };
    
    for (const char* path : dll_paths) {
        wintun_dll_ = LoadLibraryA(path);
        if (wintun_dll_) break;
    }
    
    if (!wintun_dll_) {
        last_error_ = "Failed to load wintun.dll. Please ensure Wintun is installed.";
        return false;
    }
    
    // 加载所有 Wintun API 函数
    WintunCreateAdapter_ = (WintunCreateAdapterFunc)GetProcAddress(wintun_dll_, "WintunCreateAdapter");
    WintunOpenAdapter_ = (WintunOpenAdapterFunc)GetProcAddress(wintun_dll_, "WintunOpenAdapter");
    WintunCloseAdapter_ = (WintunCloseAdapterFunc)GetProcAddress(wintun_dll_, "WintunCloseAdapter");
    WintunStartSession_ = (WintunStartSessionFunc)GetProcAddress(wintun_dll_, "WintunStartSession");
    WintunEndSession_ = (WintunEndSessionFunc)GetProcAddress(wintun_dll_, "WintunEndSession");
    WintunGetReadWaitEvent_ = (WintunGetReadWaitEventFunc)GetProcAddress(wintun_dll_, "WintunGetReadWaitEvent");
    WintunReceivePacket_ = (WintunReceivePacketFunc)GetProcAddress(wintun_dll_, "WintunReceivePacket");
    WintunReleaseReceivePacket_ = (WintunReleaseReceivePacketFunc)GetProcAddress(wintun_dll_, "WintunReleaseReceivePacket");
    WintunAllocateSendPacket_ = (WintunAllocateSendPacketFunc)GetProcAddress(wintun_dll_, "WintunAllocateSendPacket");
    WintunSendPacket_ = (WintunSendPacketFunc)GetProcAddress(wintun_dll_, "WintunSendPacket");
    WintunGetAdapterLUID_ = (WintunGetAdapterLUIDFunc)GetProcAddress(wintun_dll_, "WintunGetAdapterLUID");
    
    if (!WintunCreateAdapter_ || !WintunCloseAdapter_ || !WintunStartSession_ || 
        !WintunEndSession_ || !WintunGetReadWaitEvent_ || !WintunReceivePacket_ ||
        !WintunReleaseReceivePacket_ || !WintunAllocateSendPacket_ || !WintunSendPacket_ ||
        !WintunGetAdapterLUID_) {
        last_error_ = "Failed to load Wintun API functions";
        unload_wintun_dll();
        return false;
    }
    
    return true;
}

void TunWindows::unload_wintun_dll() {
    if (wintun_dll_) {
        FreeLibrary(wintun_dll_);
        wintun_dll_ = nullptr;
    }
    
    WintunCreateAdapter_ = nullptr;
    WintunOpenAdapter_ = nullptr;
    WintunCloseAdapter_ = nullptr;
    WintunStartSession_ = nullptr;
    WintunEndSession_ = nullptr;
    WintunGetReadWaitEvent_ = nullptr;
    WintunReceivePacket_ = nullptr;
    WintunReleaseReceivePacket_ = nullptr;
    WintunAllocateSendPacket_ = nullptr;
    WintunSendPacket_ = nullptr;
    WintunGetAdapterLUID_ = nullptr;
}

bool TunWindows::open(const std::string& device_name, uint32_t mtu) {
    if (is_open()) {
        last_error_ = "TUN device already open";
        return false;
    }

    if (!load_wintun_dll()) {
        return false;
    }

    // 准备设备名称
    std::string actual_name = device_name.empty() ? "WintunTunnel" : device_name;
    std::wstring wide_name = string_to_wstring(actual_name);
    std::wstring tunnel_type = L"ConnectTool";
    
    // 尝试打开已存在的适配器，如果不存在则创建新的
    adapter_ = WintunOpenAdapter_(wide_name.c_str());
    if (!adapter_) {
        // 创建新的适配器
        adapter_ = WintunCreateAdapter_(wide_name.c_str(), tunnel_type.c_str(), &adapter_guid_);
        if (!adapter_) {
            last_error_ = "Failed to create Wintun adapter: " + get_windows_error(GetLastError());
            unload_wintun_dll();
            return false;
        }
    }
    
    // 启动会话（使用 1MB 环形缓冲区）
    session_ = WintunStartSession_(adapter_, WINTUN_MIN_RING_CAPACITY * 4);
    if (!session_) {
        last_error_ = "Failed to start Wintun session: " + get_windows_error(GetLastError());
        WintunCloseAdapter_(adapter_);
        adapter_ = nullptr;
        unload_wintun_dll();
        return false;
    }
    
    // 获取读事件句柄
    read_event_ = WintunGetReadWaitEvent_(session_);
    if (!read_event_) {
        last_error_ = "Failed to get read wait event";
        WintunEndSession_(session_);
        WintunCloseAdapter_(adapter_);
        session_ = nullptr;
        adapter_ = nullptr;
        unload_wintun_dll();
        return false;
    }
    
    device_name_ = actual_name;
    mtu_ = mtu;
    
    return true;
}

void TunWindows::close() {
    if (session_) {
        WintunEndSession_(session_);
        session_ = nullptr;
    }
    
    if (adapter_) {
        WintunCloseAdapter_(adapter_);
        adapter_ = nullptr;
    }
    
    read_event_ = nullptr;  // 不要 CloseHandle，由 Wintun 管理
    device_name_.clear();
    
    unload_wintun_dll();
}

bool TunWindows::is_open() const {
    return adapter_ != nullptr && session_ != nullptr;
}

std::string TunWindows::get_device_name() const {
    return device_name_;
}

bool TunWindows::set_ip(const std::string& ip_address, const std::string& netmask) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return false;
    }

    // 获取适配器的 LUID
    NET_LUID luid;
    WintunGetAdapterLUID_(adapter_, &luid);
    
    // 解析 IP 地址和子网掩码
    SOCKADDR_INET addr_inet = {};
    addr_inet.Ipv4.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_address.c_str(), &addr_inet.Ipv4.sin_addr) != 1) {
        last_error_ = "Invalid IP address format";
        return false;
    }
    
    SOCKADDR_INET mask_inet = {};
    mask_inet.Ipv4.sin_family = AF_INET;
    if (inet_pton(AF_INET, netmask.c_str(), &mask_inet.Ipv4.sin_addr) != 1) {
        last_error_ = "Invalid netmask format";
        return false;
    }
    
    // 计算前缀长度
    UINT8 prefix_length = 0;
    UINT32 mask_value = ntohl(mask_inet.Ipv4.sin_addr.S_un.S_addr);
    while (mask_value & 0x80000000) {
        prefix_length++;
        mask_value <<= 1;
    }
    
    // 设置 IP 地址
    MIB_UNICASTIPADDRESS_ROW row = {};
    InitializeUnicastIpAddressEntry(&row);
    row.InterfaceLuid = luid;
    row.Address = addr_inet;
    row.OnLinkPrefixLength = prefix_length;
    row.DadState = IpDadStatePreferred;
    
    DWORD result = CreateUnicastIpAddressEntry(&row);
    if (result != NO_ERROR && result != ERROR_OBJECT_ALREADY_EXISTS) {
        last_error_ = "Failed to set IP address: " + get_windows_error(result);
        return false;
    }
    
    // 设置接口 MTU
    MIB_IPINTERFACE_ROW if_row = {};
    InitializeIpInterfaceEntry(&if_row);
    if_row.InterfaceLuid = luid;
    if_row.Family = AF_INET;
    
    result = GetIpInterfaceEntry(&if_row);
    if (result == NO_ERROR) {
        if_row.NlMtu = mtu_;
        if_row.SitePrefixLength = 0;
        result = SetIpInterfaceEntry(&if_row);
        if (result != NO_ERROR) {
            last_error_ = "Warning: Failed to set MTU: " + get_windows_error(result);
            // 不返回失败，MTU 设置失败不是致命错误
        }
    }
    
    return true;
}

bool TunWindows::set_up() {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return false;
    }

    // Windows 上的 Wintun 适配器创建后默认是 UP 状态
    return true;
}

int TunWindows::read(uint8_t* buffer, size_t max_length) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return -1;
    }

    // 非阻塞模式：检查是否有数据可读
    if (non_blocking_) {
        DWORD wait_result = WaitForSingleObject(read_event_, 0);
        if (wait_result == WAIT_TIMEOUT) {
            return 0;  // 没有数据
        } else if (wait_result != WAIT_OBJECT_0) {
            last_error_ = "Wait for read event failed";
            return -1;
        }
    }
    
    // 接收数据包
    DWORD packet_size = 0;
    BYTE* packet = WintunReceivePacket_(session_, &packet_size);
    
    if (!packet) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_MORE_ITEMS) {
            // 没有数据包可读
            if (non_blocking_) {
                return 0;
            }
            // 阻塞模式：等待数据
            WaitForSingleObject(read_event_, INFINITE);
            return 0;  // 重试
        } else if (error == ERROR_HANDLE_EOF) {
            last_error_ = "Wintun adapter is terminating";
            return -1;
        } else if (error == ERROR_INVALID_DATA) {
            last_error_ = "Wintun buffer is corrupt";
            return -1;
        } else {
            last_error_ = "Receive packet failed: " + get_windows_error(error);
            return -1;
        }
    }
    
    // 检查缓冲区大小
    if (packet_size > max_length) {
        last_error_ = "Buffer too small for packet";
        WintunReleaseReceivePacket_(session_, packet);
        return -1;
    }
    
    // 复制数据
    memcpy(buffer, packet, packet_size);
    
    // 释放数据包
    WintunReleaseReceivePacket_(session_, packet);
    
    return static_cast<int>(packet_size);
}

int TunWindows::write(const uint8_t* buffer, size_t length) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return -1;
    }

    if (length > 0xFFFF) {  // WINTUN_MAX_IP_PACKET_SIZE
        last_error_ = "Packet too large";
        return -1;
    }
    
    // 分配发送缓冲区
    BYTE* packet = WintunAllocateSendPacket_(session_, static_cast<DWORD>(length));
    if (!packet) {
        DWORD error = GetLastError();
        if (error == ERROR_BUFFER_OVERFLOW) {
            if (non_blocking_) {
                return 0;  // 缓冲区满，非阻塞模式返回 0
            }
            last_error_ = "Wintun buffer is full";
            return -1;
        } else if (error == ERROR_HANDLE_EOF) {
            last_error_ = "Wintun adapter is terminating";
            return -1;
        } else {
            last_error_ = "Allocate send packet failed: " + get_windows_error(error);
            return -1;
        }
    }
    
    // 复制数据
    memcpy(packet, buffer, length);
    
    // 发送数据包
    WintunSendPacket_(session_, packet);
    
    return static_cast<int>(length);
}

std::string TunWindows::get_last_error() const {
    return last_error_;
}

uint32_t TunWindows::get_mtu() const {
    return mtu_;
}

bool TunWindows::set_non_blocking(bool non_blocking) {
    if (!is_open()) {
        last_error_ = "TUN device not open";
        return false;
    }

    non_blocking_ = non_blocking;
    return true;
}

} // namespace tun

#endif // _WIN32
