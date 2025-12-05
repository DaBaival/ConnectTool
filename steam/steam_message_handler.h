#ifndef STEAM_MESSAGE_HANDLER_H
#define STEAM_MESSAGE_HANDLER_H

// ============================================================================
// Steam 网络消息处理器
// ============================================================================

// 标准库头文件
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

// 第三方库头文件
#include <asio.hpp>

// Steam 头文件 - 前向声明接口
class ISteamNetworkingMessages;

// 前向声明
class SteamNetworkingManager;

/**
 * @brief Steam 网络消息处理器（ISteamNetworkingMessages 版本）
 * 
 * 使用 Asio 定时器实现高效的消息轮询，支持自适应轮询间隔：
 * - 有消息时：使用最小轮询间隔 (0.1ms) 保证低延迟
 * - 无消息时：逐步增加轮询间隔，最大到 1ms，减少 CPU 占用
 * 
 * 支持两种运行模式：
 * 1. 内部模式：创建独立的 io_context 和运行线程
 * 2. 外部模式：使用外部提供的 io_context（调用 setIoContext）
 */
class SteamMessageHandler {
public:
    /**
     * @brief 构造函数
     * @param interface Steam 消息接口指针
     * @param manager   网络管理器指针
     */
    SteamMessageHandler(ISteamNetworkingMessages* interface, 
                        SteamNetworkingManager* manager);
    ~SteamMessageHandler();

    // 禁用拷贝和移动
    SteamMessageHandler(const SteamMessageHandler&) = delete;
    SteamMessageHandler& operator=(const SteamMessageHandler&) = delete;
    SteamMessageHandler(SteamMessageHandler&&) = delete;
    SteamMessageHandler& operator=(SteamMessageHandler&&) = delete;

    /**
     * @brief 启动消息处理器
     * @note 如果使用内部 io_context，会自动创建运行线程
     */
    void start();

    /**
     * @brief 停止消息处理器
     * @note 会等待内部线程结束后返回
     */
    void stop();
    
    /**
     * @brief 设置外部 io_context
     * @param externalContext 外部 io_context 指针
     * @note 必须在 start() 之前调用
     */
    void setIoContext(asio::io_context* externalContext);

    /**
     * @brief 检查是否正在运行
     */
    bool isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    // 轮询控制
    void schedulePoll();
    void pollMessages();
    void runInternalLoop();

    // Steam 接口
    ISteamNetworkingMessages* messagesInterface_;
    SteamNetworkingManager* manager_;

    // Asio 组件
    std::unique_ptr<asio::io_context> internalIoContext_;
    asio::io_context* ioContext_;
    std::unique_ptr<asio::steady_timer> pollTimer_;
    std::unique_ptr<std::thread> ioThread_;
    
    // 状态
    std::atomic<bool> running_{false};
    std::chrono::microseconds currentPollInterval_;
    
    // 常量
    static constexpr int kVpnChannel = 0;
    static constexpr auto kMinPollInterval = std::chrono::microseconds{100};   // 0.1ms
    static constexpr auto kMaxPollInterval = std::chrono::microseconds{1000};  // 1ms
    static constexpr auto kPollIncrement = std::chrono::microseconds{100};     // 0.1ms
    static constexpr int kMaxMessagesPerPoll = 64;
};

#endif // STEAM_MESSAGE_HANDLER_H