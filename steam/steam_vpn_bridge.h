#ifndef STEAM_VPN_BRIDGE_H
#define STEAM_VPN_BRIDGE_H

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <steam_api.h>
#include <isteamnetworkingmessages.h>

#include "../tun/tun_interface.h"
#include "../vpn/vpn_protocol.h"

// Forward declarations
class SteamNetworkingManager;

/**
 * @brief Steam VPN桥接器（ISteamNetworkingMessages 版本）
 * 
 * 负责在虚拟网卡和Steam网络之间转发IP数据包
 * 使用 ISteamNetworkingMessages 实现无连接的消息传递
 * 使用 Steam Fake IP 进行寻址
 */
class SteamVpnBridge {
public:
    SteamVpnBridge(SteamNetworkingManager* steamManager);
    ~SteamVpnBridge();

    /**
     * @brief 启动VPN桥接
     * @param tunDeviceName TUN设备名称（可选）
     * @param virtualSubnet 虚拟子网（可选，FakeIP模式下忽略）
     * @param subnetMask 子网掩码（可选，FakeIP模式下忽略）
     * @return true 成功，false 失败
     */
    bool start(const std::string& tunDeviceName = "", 
               const std::string& virtualSubnet = "",
               const std::string& subnetMask = "");

    /**
     * @brief 停止VPN桥接
     */
    void stop();

    /**
     * @brief 检查VPN是否正在运行
     */
    bool isRunning() const { return running_; }

    /**
     * @brief 获取本地分配的IP地址
     */
    std::string getLocalIP() const;

    /**
     * @brief 获取TUN设备名称
     */
    std::string getTunDeviceName() const;

    /**
     * @brief 获取路由表 (Empty in FakeIP mode)
     */
    std::map<uint32_t, RouteEntry> getRoutingTable() const;

    /**
     * @brief 处理来自Steam的VPN消息（使用 SteamID 标识发送者）
     * @param data 消息数据
     * @param length 消息长度
     * @param senderSteamID 发送者的 Steam ID
     */
    void handleVpnMessage(const uint8_t* data, size_t length, CSteamID senderSteamID);

    /**
     * @brief 当新用户加入时
     * @param steamID 用户的Steam ID
     */
    void onUserJoined(CSteamID steamID);

    /**
     * @brief 当用户离开时清理路由
     * @param steamID 用户的Steam ID
     */
    void onUserLeft(CSteamID steamID);

    /**
     * @brief 获取统计信息
     */
    struct Statistics {
        uint64_t packetsSent;
        uint64_t packetsReceived;
        uint64_t bytesSent;
        uint64_t bytesReceived;
        uint64_t packetsDropped;
    };
    Statistics getStatistics() const;

private:
    // TUN设备读取线程
    void tunReadThread();

    // 发送 VPN 消息（使用 ISteamNetworkingMessages）
    void sendVpnMessage(VpnMessageType type, const uint8_t* payload, size_t payloadLength, 
                        CSteamID targetSteamID, bool reliable = true);
    void broadcastVpnMessage(VpnMessageType type, const uint8_t* payload, size_t payloadLength, 
                             bool reliable = true);

    // IP Query/Response
    void sendIpQuery(CSteamID target = k_steamIDNil);
    void sendIpResponse(CSteamID target);

    // Steam网络管理器
    SteamNetworkingManager* steamManager_;

    // TUN设备
    std::unique_ptr<tun::TunInterface> tunDevice_;

    // 运行状态
    std::atomic<bool> running_;

    // TUN读取线程
    std::unique_ptr<std::thread> tunReadThread_;

    // IP地址池配置
    uint32_t localIP_;

    // Routing Table: FakeIP -> SteamID
    // Used to resolve destination SteamID when Steam's internal resolution fails
    // or for optimization.
    std::map<uint32_t, CSteamID> routingTable_;
    mutable std::mutex routingMutex_;

    // 统计信息
    Statistics stats_;
    mutable std::mutex statsMutex_;
};

#endif // STEAM_VPN_BRIDGE_H