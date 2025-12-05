#include "steam_vpn_bridge.h"
#include "steam_networking_manager.h"
#include "config/config_manager.h"
#include "steam_vpn_utils.h"
#include "../vpn/vpn_utils.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

using namespace SteamVpnUtils;
using namespace VpnUtils;

SteamVpnBridge::SteamVpnBridge(SteamNetworkingManager* steamManager)
    : steamManager_(steamManager)
    , running_(false)
    , localIP_(0)
{
    memset(&stats_, 0, sizeof(stats_));
}

SteamVpnBridge::~SteamVpnBridge() {
    stop();
}

bool SteamVpnBridge::start(const std::string& tunDeviceName,
                            const std::string& virtualSubnet,
                            const std::string& subnetMask) {
    if (running_) {
        std::cerr << "VPN bridge is already running" << std::endl;
        return false;
    }

    // Retrieve Fake IP from manager
    localIP_ = steamManager_->getLocalFakeIP();
    if (localIP_ == 0) {
        std::cerr << "Cannot start VPN: No Fake IP assigned yet." << std::endl;
        return false;
    }

    const auto& config = ConfigManager::instance().getConfig();
    int steamMtuDataSize = querySteamMtuDataSize();
    int mtu = calculateTunMtu(steamMtuDataSize);
    
    if (config.vpn.default_mtu > 0 && config.vpn.default_mtu < mtu) {
        std::cout << "[MTU] Using config MTU (" << config.vpn.default_mtu 
                  << ") instead of calculated (" << mtu << ")" << std::endl;
        mtu = config.vpn.default_mtu;
    }

    tunDevice_ = tun::create_tun();
    if (!tunDevice_) {
        std::cerr << "Failed to create TUN device" << std::endl;
        return false;
    }

    if (!tunDevice_->open(tunDeviceName, mtu)) { 
        std::cerr << "Failed to open TUN device: " << tunDevice_->get_last_error() << std::endl;
        return false;
    }

    std::cout << "TUN device created: " << tunDevice_->get_device_name() << std::endl;

    if (!tunDevice_->set_mtu(mtu)) {
        std::cerr << "Failed to set TUN device MTU: " << tunDevice_->get_last_error() << std::endl;
        return false;
    }

    std::cout << "TUN device MTU set to: " << mtu << std::endl;

    // Configure TUN IP
    // Use the assigned Fake IP.
    // For subnet, we use a standard /16 for 169.254.x.x or whatever Steam assigns.
    // Steam Fake IPs are typically 169.254.x.x/16.
    std::string localIPStr = ipToString(localIP_);
    std::string subnetMaskStr = "255.255.0.0"; // Default for Link-Local

    if (tunDevice_->set_ip(localIPStr, subnetMaskStr) && tunDevice_->set_up(true)) {
        std::cout << "TUN device configured with IP: " << localIPStr << " Mask: " << subnetMaskStr << std::endl;
    } else {
        std::cerr << "Failed to configure TUN device IP." << std::endl;
        return false;
    }

    tunDevice_->set_non_blocking(false);

    running_ = true;
    tunReadThread_ = std::make_unique<std::thread>(&SteamVpnBridge::tunReadThread, this);
    
    // Broadcast IP Query to discover peers
    // Retry a few times to ensure we catch members if the list populates slowly
    std::thread([this]() {
        for (int i = 0; i < 4; ++i) {
            if (!this->running_) break;
            this->sendIpQuery();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();

    std::cout << "Steam VPN bridge started successfully" << std::endl;
    return true;
}

void SteamVpnBridge::stop() {
    if (!running_) return;

    running_ = false;

    if (tunReadThread_ && tunReadThread_->joinable()) {
        tunReadThread_->join();
    }

    if (tunDevice_) {
        tunDevice_->close();
    }

    localIP_ = 0;
    
    {
        std::lock_guard<std::mutex> lock(routingMutex_);
        routingTable_.clear();
    }

    std::cout << "Steam VPN bridge stopped" << std::endl;
}

std::string SteamVpnBridge::getLocalIP() const {
    if (localIP_ == 0) return "Not assigned";
    return ipToString(localIP_);
}

std::string SteamVpnBridge::getTunDeviceName() const {
    if (tunDevice_ && tunDevice_->is_open()) {
        return tunDevice_->get_device_name();
    }
    return "N/A";
}

std::map<uint32_t, RouteEntry> SteamVpnBridge::getRoutingTable() const {
    std::lock_guard<std::mutex> lock(routingMutex_);
    std::map<uint32_t, RouteEntry> result;
    
    for (const auto& pair : routingTable_) {
        RouteEntry entry;
        entry.ipAddress = pair.first;
        entry.steamID = pair.second;
        entry.isLocal = false;
        // name and nodeId are not strictly tracked here, but could be added if needed
        if (SteamFriends()) {
            entry.name = SteamFriends()->GetFriendPersonaName(pair.second);
        } else {
            entry.name = "Unknown";
        }
        result[pair.first] = entry;
    }
    return result;
}

void SteamVpnBridge::tunReadThread() {
    std::cout << "TUN read thread started" << std::endl;
    
    constexpr size_t BUFFER_SIZE = 16384;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    
    while (running_) {
        int bytesRead = tunDevice_->read(buffer.data(), buffer.size());
        
        if (bytesRead > 0) {
            uint32_t destIP = extractDestIP(buffer.data(), bytesRead);
            
            // 1. Try local routing table first
            CSteamID targetSteamID = k_steamIDNil;
            {
                std::lock_guard<std::mutex> lock(routingMutex_);
                auto it = routingTable_.find(destIP);
                if (it != routingTable_.end()) {
                    targetSteamID = it->second;
                }
            }

            // 2. If not found, try Steam's FakeIP resolution
            if (targetSteamID == k_steamIDNil) {
                SteamNetworkingIPAddr fakeIP;
                fakeIP.SetIPv4(destIP, 0); // Port 0
                
                SteamNetworkingIdentity identity;
                EResult result = SteamNetworkingUtils()->GetRealIdentityForFakeIP(fakeIP, &identity);
                
                if (result == k_EResultOK) {
                    targetSteamID = identity.GetSteamID();
                    // Cache it? Maybe not, let the protocol handle updates.
                }
            }
            
            if (targetSteamID != k_steamIDNil) {
                // Backpressure check
                int pendingBytes = steamManager_->getPendingSendBytes(targetSteamID);
                int retryCount = 0;
                // Allow up to 128KB buffer, wait max 50ms
                const int MAX_PENDING_BYTES = 128 * 1024; 
                const int MAX_RETRIES = 50; 
                
                while (pendingBytes > MAX_PENDING_BYTES) { 
                     if (retryCount >= MAX_RETRIES) {
                         // std::cout << "[VPN] Dropping packet to " << targetSteamID.ConvertToUint64() 
                         //           << " due to buffer overflow (" << pendingBytes << " bytes pending)" << std::endl;
                         goto skip_send; 
                     }
                     
                     std::this_thread::sleep_for(std::chrono::milliseconds(1));
                     pendingBytes = steamManager_->getPendingSendBytes(targetSteamID);
                     retryCount++;
                }

                // Found a real identity
                sendVpnMessage(VpnMessageType::IP_PACKET, buffer.data(), bytesRead, targetSteamID, false);
                
                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.packetsSent++;
                    stats_.bytesSent += bytesRead;
                }

                skip_send:;
            } else {
                // Unknown Fake IP or Broadcast
                // Check for global broadcast (255.255.255.255) or Link-Local broadcast (169.254.255.255)
                // Since FakeIP is 169.254.0.0/16, the broadcast is 169.254.255.255.
                if (destIP == 0xFFFFFFFF || destIP == 0xA9FEFFFF) { // 255.255.255.255 or 169.254.255.255
                     broadcastVpnMessage(VpnMessageType::IP_PACKET, buffer.data(), bytesRead, false);
                     
                     auto members = steamManager_->getRoomMembers();
                     std::lock_guard<std::mutex> lock(statsMutex_);
                     stats_.packetsSent += members.size();
                     stats_.bytesSent += bytesRead * members.size();
                }
            }
        }
    }
    
    std::cout << "TUN read thread stopped" << std::endl;
}

void SteamVpnBridge::handleVpnMessage(const uint8_t* data, size_t length, CSteamID senderSteamID) {
    if (length < sizeof(VpnMessageHeader)) return;

    VpnMessageHeader header;
    memcpy(&header, data, sizeof(VpnMessageHeader));
    uint16_t payloadLength = ntohs(header.length);

    if (length < sizeof(VpnMessageHeader) + payloadLength) return;

    const uint8_t* payload = data + sizeof(VpnMessageHeader);
    
    switch (header.type) {
        case VpnMessageType::IP_PACKET: {
            if (tunDevice_) {
                // Write directly to TUN
                tunDevice_->write(payload, payloadLength);
                
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.packetsReceived++;
                stats_.bytesReceived += payloadLength;
            }
            break;
        }
        case VpnMessageType::IP_QUERY: {
            if (payloadLength >= sizeof(IpQueryPayload)) {
                IpQueryPayload query;
                memcpy(&query, payload, sizeof(IpQueryPayload));
                
                // Learn sender's IP immediately
                if (query.ipAddress != 0) {
                     std::lock_guard<std::mutex> lock(routingMutex_);
                     routingTable_[query.ipAddress] = senderSteamID;
                     
                     char szIP[64];
                     SteamNetworkingIPAddr ipAddr;
                     ipAddr.SetIPv4(query.ipAddress, 0);
                     ipAddr.ToString(szIP, sizeof(szIP), false);
                     std::cout << "[VPN] Learned IP from Query: " << szIP << " -> " << senderSteamID.ConvertToUint64() << std::endl;
                }
            }

            // Respond with our IP
            sendIpResponse(senderSteamID);
            break;
        }
        case VpnMessageType::IP_RESPONSE: {
            if (payloadLength >= sizeof(IpResponsePayload)) {
                IpResponsePayload response;
                memcpy(&response, payload, sizeof(IpResponsePayload));
                
                std::lock_guard<std::mutex> lock(routingMutex_);
                routingTable_[response.ipAddress] = senderSteamID;
                
                char szIP[64];
                SteamNetworkingIPAddr ipAddr;
                ipAddr.SetIPv4(response.ipAddress, 0);
                ipAddr.ToString(szIP, sizeof(szIP), false);
                
                std::cout << "[VPN] Learned IP from Response: " << szIP << " -> " << senderSteamID.ConvertToUint64() << std::endl;
            }
            break;
        }
        default:
            break;
    }
}

void SteamVpnBridge::onUserJoined(CSteamID steamID) {
    std::cout << "User joined: " << steamID.ConvertToUint64() << std::endl;
    // Send IP Query to the new user to get their IP
    sendIpQuery(steamID);
}

void SteamVpnBridge::onUserLeft(CSteamID steamID) {
    std::cout << "User left: " << steamID.ConvertToUint64() << std::endl;
    
    std::lock_guard<std::mutex> lock(routingMutex_);
    for (auto it = routingTable_.begin(); it != routingTable_.end(); ) {
        if (it->second == steamID) {
            it = routingTable_.erase(it);
        } else {
            ++it;
        }
    }
}

SteamVpnBridge::Statistics SteamVpnBridge::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void SteamVpnBridge::sendVpnMessage(VpnMessageType type, const uint8_t* payload, 
                                     size_t payloadLength, CSteamID targetSteamID, bool reliable) {
    std::vector<uint8_t> message;
    VpnMessageHeader header;
    header.type = type;
    header.length = htons(static_cast<uint16_t>(payloadLength));
    
    message.resize(sizeof(VpnMessageHeader) + payloadLength);
    memcpy(message.data(), &header, sizeof(VpnMessageHeader));
    if (payloadLength > 0 && payload) {
        memcpy(message.data() + sizeof(VpnMessageHeader), payload, payloadLength);
    }
    
    int flags = reliable ? k_nSteamNetworkingSend_Reliable : 
                           (k_nSteamNetworkingSend_UnreliableNoNagle | k_nSteamNetworkingSend_NoDelay);
    steamManager_->sendMessageToUser(targetSteamID, message.data(), 
        static_cast<uint32_t>(message.size()), flags);
}

void SteamVpnBridge::broadcastVpnMessage(VpnMessageType type, const uint8_t* payload, 
                                          size_t payloadLength, bool reliable) {
    std::vector<uint8_t> message;
    VpnMessageHeader header;
    header.type = type;
    header.length = htons(static_cast<uint16_t>(payloadLength));
    
    message.resize(sizeof(VpnMessageHeader) + payloadLength);
    memcpy(message.data(), &header, sizeof(VpnMessageHeader));
    if (payloadLength > 0 && payload) {
        memcpy(message.data() + sizeof(VpnMessageHeader), payload, payloadLength);
    }
    
    int flags = reliable ? k_nSteamNetworkingSend_Reliable : 
                           (k_nSteamNetworkingSend_UnreliableNoNagle | k_nSteamNetworkingSend_NoDelay);
    steamManager_->broadcastMessage(message.data(), static_cast<uint32_t>(message.size()), flags);
}

void SteamVpnBridge::sendIpQuery(CSteamID target) {
    IpQueryPayload payload;
    payload.ipAddress = localIP_;  // Include local IP
    // Fill payload if needed, currently empty/NodeID only
    
    if (target == k_steamIDNil) {
        std::cout << "[VPN] Broadcasting IP Query..." << std::endl;
        broadcastVpnMessage(VpnMessageType::IP_QUERY, reinterpret_cast<uint8_t*>(&payload), sizeof(payload), true);
    } else {
        std::cout << "[VPN] Sending IP Query to " << target.ConvertToUint64() << std::endl;
        sendVpnMessage(VpnMessageType::IP_QUERY, reinterpret_cast<uint8_t*>(&payload), sizeof(payload), target, true);
    }
}

void SteamVpnBridge::sendIpResponse(CSteamID target) {
    IpResponsePayload payload;
    payload.ipAddress = localIP_;
    // NodeID could be added here if we were using it
    
    std::cout << "[VPN] Sending IP Response to " << target.ConvertToUint64() << std::endl;
    sendVpnMessage(VpnMessageType::IP_RESPONSE, reinterpret_cast<uint8_t*>(&payload), sizeof(payload), target, true);
}