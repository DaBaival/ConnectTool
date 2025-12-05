#ifndef VPN_PROTOCOL_H
#define VPN_PROTOCOL_H

#include <cstdint>
#include <array>
#include <chrono>
#include <string>
#include <steam_api.h>
#include <isteamnetworkingsockets.h>

// ============================================================================
// VPN Protocol Constants
// ============================================================================

// Steam Networking Messages API message size limits
// Unreliable message limit is ~1200 bytes, Reliable is ~512KB
// We use Unreliable for IP packets to reduce latency
constexpr size_t STEAM_UNRELIABLE_MSG_SIZE_LIMIT = 1200;

// VPN Message Overhead
// VpnMessageHeader (3 bytes) + VpnPacketWrapper (32 bytes NodeID) = 35 bytes
constexpr size_t VPN_MESSAGE_OVERHEAD = 35;

// Recommended MTU: Steam limit - Overhead - Safety margin
// 1200 - 35 - 65 = 1100 (65 bytes safety margin)
constexpr int RECOMMENDED_MTU = 1100;

// Node ID Size (SHA-256 = 32 bytes = 256 bits)
constexpr size_t NODE_ID_SIZE = 32;

// ============================================================================
// Type Definitions
// ============================================================================
using NodeID = std::array<uint8_t, NODE_ID_SIZE>;

// ============================================================================
// VPN Message Types
// ============================================================================
enum class VpnMessageType : uint8_t {
    IP_PACKET = 1,              // IP Packet (contains sender Node ID)
    ROUTE_UPDATE = 3,           // Route Table Update (Legacy, kept for compatibility if needed)
    
    // IP Discovery Protocol
    IP_QUERY = 4,               // Query for IP address
    IP_RESPONSE = 5,            // Response with IP address
};

// ============================================================================
// Protocol Message Structures
// ============================================================================
#pragma pack(push, 1)

/**
 * @brief VPN Message Header
 */
struct VpnMessageHeader {
    VpnMessageType type;    // Message Type
    uint16_t length;        // Data Length
};

/**
 * @brief IP Packet Wrapper (contains sender Node ID)
 * Used for packet-level collision detection
 */
struct VpnPacketWrapper {
    NodeID senderNodeId;    // Sender's Node ID
    // Followed by actual IP packet
};

/**
 * @brief IP Query Payload
 * Sent when a node joins to discover other nodes' IPs
 */
struct IpQueryPayload {
    uint32_t ipAddress;     // Sender's IP Address (Network Byte Order)
    NodeID senderNodeId;    // Sender's Node ID
};

/**
 * @brief IP Response Payload
 * Sent in response to IP_QUERY, or to announce presence
 */
struct IpResponsePayload {
    uint32_t ipAddress;     // Local FakeIP (Network Byte Order)
    NodeID nodeId;          // Sender's Node ID
    // SteamID is implicit in the message source
};

#pragma pack(pop)

// ============================================================================
// Node Information
// ============================================================================

/**
 * @brief Node Information
 */
struct NodeInfo {
    NodeID nodeId;                                      // 256-bit Node ID
    CSteamID steamId;                                   // Steam ID
    uint32_t ipAddress;                                 // Assigned IP Address
    std::string name;                                   // User Name
    bool isLocal;                                       // Is Local Node
};

/**
 * @brief IP Route Entry
 */
struct RouteEntry {
    CSteamID steamID;           // Corresponding Steam ID
    uint32_t ipAddress;         // IP Address (Host Byte Order)
    std::string name;           // User Name
    bool isLocal;               // Is Local Node
    NodeID nodeId;              // Node ID
};

#endif // VPN_PROTOCOL_H