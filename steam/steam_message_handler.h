#ifndef STEAM_MESSAGE_HANDLER_H
#define STEAM_MESSAGE_HANDLER_H

#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <steamnetworkingtypes.h>
#include <isteamnetworkingsockets.h>

class SteamMessageHandler {
public:
    SteamMessageHandler(ISteamNetworkingSockets* interface, std::vector<HSteamNetConnection>& connections, std::mutex& connectionsMutex);
    ~SteamMessageHandler();

    void start();
    void stop();

private:
    void pollLoop();

    ISteamNetworkingSockets* m_pInterface_;
    std::vector<HSteamNetConnection>& connections_;
    std::mutex& connectionsMutex_;

    std::unique_ptr<std::thread> pollThread_;
    bool running_;
    int currentPollInterval_; // 当前轮询间隔（毫秒）
};

#endif // STEAM_MESSAGE_HANDLER_H