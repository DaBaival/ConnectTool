#pragma once

#include "tun_interface.h"

#ifdef _WIN32

#include <windows.h>
#include <guiddef.h>

// 前置声明 Wintun 类型
typedef struct _WINTUN_ADAPTER *WINTUN_ADAPTER_HANDLE;
typedef struct _TUN_SESSION *WINTUN_SESSION_HANDLE;

namespace tun {

/**
 * @brief Windows 平台的 TUN 实现
 * 
 * 使用 Wintun 驱动（高性能，现代化）
 * 需要 wintun.dll（从 third_party/wintun 动态加载）
 */
class TunWindows : public TunInterface {
public:
    TunWindows();
    ~TunWindows() override;

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
    WINTUN_ADAPTER_HANDLE adapter_;     // Wintun 适配器句柄
    WINTUN_SESSION_HANDLE session_;     // Wintun 会话句柄
    std::string device_name_;           // 设备名称
    uint32_t mtu_;                      // MTU
    std::string last_error_;            // 最后的错误信息
    bool non_blocking_;                 // 是否非阻塞
    HANDLE read_event_;                 // 读事件（用于非阻塞）
    GUID adapter_guid_;                 // 适配器 GUID
    
    // Wintun DLL 句柄
    HMODULE wintun_dll_;
    
    // Wintun API 函数指针
    typedef WINTUN_ADAPTER_HANDLE (WINAPI *WintunCreateAdapterFunc)(LPCWSTR, LPCWSTR, const GUID*);
    typedef WINTUN_ADAPTER_HANDLE (WINAPI *WintunOpenAdapterFunc)(LPCWSTR);
    typedef VOID (WINAPI *WintunCloseAdapterFunc)(WINTUN_ADAPTER_HANDLE);
    typedef WINTUN_SESSION_HANDLE (WINAPI *WintunStartSessionFunc)(WINTUN_ADAPTER_HANDLE, DWORD);
    typedef VOID (WINAPI *WintunEndSessionFunc)(WINTUN_SESSION_HANDLE);
    typedef HANDLE (WINAPI *WintunGetReadWaitEventFunc)(WINTUN_SESSION_HANDLE);
    typedef BYTE* (WINAPI *WintunReceivePacketFunc)(WINTUN_SESSION_HANDLE, DWORD*);
    typedef VOID (WINAPI *WintunReleaseReceivePacketFunc)(WINTUN_SESSION_HANDLE, const BYTE*);
    typedef BYTE* (WINAPI *WintunAllocateSendPacketFunc)(WINTUN_SESSION_HANDLE, DWORD);
    typedef VOID (WINAPI *WintunSendPacketFunc)(WINTUN_SESSION_HANDLE, const BYTE*);
    typedef VOID (WINAPI *WintunGetAdapterLUIDFunc)(WINTUN_ADAPTER_HANDLE, void*);
    
    WintunCreateAdapterFunc WintunCreateAdapter_;
    WintunOpenAdapterFunc WintunOpenAdapter_;
    WintunCloseAdapterFunc WintunCloseAdapter_;
    WintunStartSessionFunc WintunStartSession_;
    WintunEndSessionFunc WintunEndSession_;
    WintunGetReadWaitEventFunc WintunGetReadWaitEvent_;
    WintunReceivePacketFunc WintunReceivePacket_;
    WintunReleaseReceivePacketFunc WintunReleaseReceivePacket_;
    WintunAllocateSendPacketFunc WintunAllocateSendPacket_;
    WintunSendPacketFunc WintunSendPacket_;
    WintunGetAdapterLUIDFunc WintunGetAdapterLUID_;
    
    bool load_wintun_dll();
    void unload_wintun_dll();
    std::wstring string_to_wstring(const std::string& str);
    std::string get_windows_error(DWORD error_code);
};

} // namespace tun

#endif // _WIN32
