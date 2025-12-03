# ConnectTool - 在线游戏联机工具

基于 Steam 网络的在线游戏联机工具，支持创建和加入游戏房间，提供 P2P 网络连接和 TCP 端口转发功能。使用 Dear ImGui 构建图形界面。

## 功能特性

- **Steam 网络集成**: 基于 Steamworks SDK 实现 P2P 网络连接
- **房间管理**: 创建和加入游戏房间，支持邀请 Steam 好友
- **TCP 服务器**: 内置 TCP 服务器，监听端口 8888，支持多客户端连接
- **连接状态监控**: 实时显示房间成员、延迟和连接类型
- **单实例运行**: 确保只有一个程序实例运行，自动激活已存在的窗口
- **跨平台支持**: 支持 Windows、Linux 和 macOS

## 系统要求

- C++17 兼容编译器
- CMake 3.10 或更高版本
- OpenGL 3.0 或更高版本
- Steam 客户端（需要登录）
- 以下依赖库：
  - GLFW3
  - Boost (system 组件)
  - Steamworks SDK

## 依赖项说明

### Dear ImGui
1. 从 [Dear ImGui](https://github.com/ocornut/imgui) 克隆或下载
2. 将内容放置到项目根目录的 `imgui/` 文件夹
3. 或使用 git submodule 添加：
   ```bash
   git submodule add https://github.com/ocornut/imgui.git imgui
   ```

### nanoid_cpp
1. 从 [nanoid_cpp](https://github.com/mcmikecreations/nanoid_cpp) 克隆或下载
2. 将内容放置到项目根目录的 `nanoid_cpp/` 文件夹
3. 或使用 git submodule 添加：
   ```bash
   git submodule add https://github.com/mcmikecreations/nanoid_cpp.git nanoid_cpp
   ```

用于生成唯一的、URL 友好的字符串 ID。

### Steamworks SDK
1. 从 [Steamworks SDK](https://partner.steamgames.com/) 下载
2. 解压到项目根目录的 `steamworks/` 文件夹

### 中文字体
程序需要 `font.ttf` 文件以显示中文界面，请将支持中文的 TrueType 字体文件放置在可执行文件同级目录。

## 构建步骤

### Windows (使用 vcpkg)

1. 安装依赖:
   ```powershell
   vcpkg install glfw3 boost-system
   ```

2. 配置并构建:
   ```powershell
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg路径]/scripts/buildsystems/vcpkg.cmake
   cmake --build . --config Release
   ```

3. 运行程序:
   ```powershell
   .\Release\OnlineGameTool.exe
   ```

### Linux

1. 安装依赖:
   ```bash
   sudo apt install libglfw3-dev libboost-system-dev
   ```

2. 构建:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. 运行:
   ```bash
   ./OnlineGameTool
   ```

### macOS

1. 安装依赖:
   ```bash
   brew install glfw boost
   ```

2. 构建和运行步骤同 Linux

## 使用说明

1. **启动程序**: 确保 Steam 客户端已登录
2. **主持房间**: 点击"主持游戏房间"按钮创建新房间
3. **加入房间**: 输入房间 ID 并点击"加入游戏房间"
4. **邀请好友**: 在好友列表中选择好友发送邀请
5. **查看状态**: 在"房间状态"窗口查看所有成员的连接信息

## 项目结构

```
ConnectTool/
├── ConnectTool/
│   ├── online_game_tool.cpp    # 主程序
│   ├── net/                    # 网络模块
│   │   ├── tcp_server.cpp     # TCP 服务器实现
│   │   └── multiplex_manager.cpp
│   └── steam/                  # Steam 网络模块
│       ├── steam_networking_manager.cpp
│       ├── steam_room_manager.cpp
│       ├── steam_message_handler.cpp
│       └── steam_utils.cpp
├── imgui/                      # Dear ImGui 库
├── nanoid_cpp/                 # ID 生成库
├── steamworks/                 # Steamworks SDK
└── CMakeLists.txt
```

## 技术栈

- **UI 框架**: Dear ImGui + GLFW + OpenGL3
- **网络**: Steamworks P2P + Boost.Asio
- **构建系统**: CMake
- **语言标准**: C++17

## 配置系统

程序**必须**从远程服务器加载配置才能启动。配置中包含最低版本要求，如果应用版本低于要求则会退出。

### 配置文件 URL

程序会按顺序尝试以下地址加载配置，直到成功：

1. GitHub 原始地址
2. gh-proxy.org 主站（Cloudflare 全球加速）
3. 香港节点（国内线路优化）
4. Fastly CDN
5. EdgeOne 全球加速
6. IPv6 支持节点

如果所有地址都无法访问，程序将退出。

### 配置项说明

```json
{
    "min_version": "1.0.0",
    "app": {
        "name": "ConnectTool",
        "steam_app_id": 480
    },
    "vpn": {
        "virtual_subnet": "10.0.0.0",
        "subnet_mask": "255.0.0.0",
        "default_mtu": 1400,
        "tun_device_name": "SteamVPN"
    },
    "protocol": {
        "app_secret_salt": "ConnectTool_VPN_Salt_v1",
        "probe_timeout_ms": 500,
        "heartbeat_interval_ms": 60000,
        "lease_time_ms": 120000,
        "lease_expiry_ms": 360000,
        "heartbeat_expiry_ms": 180000
    },
    "networking": {
        "send_rate_mb": 50,
        "send_buffer_size_mb": 4,
        "nagle_time": 0,
        "steam_callback_interval_ms": 10
    },
    "server": {
        "unix_socket_path_windows": "connect_tool.sock",
        "unix_socket_path_unix": "/tmp/connect_tool.sock"
    }
}
```

### 版本检查

- `min_version`: 最低版本要求
- 如果应用版本低于此值，程序将显示错误并退出
- 请确保始终使用最新版本

## 注意事项

- 程序运行时需要 Steam 客户端处于登录状态
- TCP 服务器默认监听端口 8888，请确保端口未被占用
- 首次运行需要将 `steam_api64.dll` (Windows) 及相应的动态库文件放在可执行文件同级目录

## 致谢

感谢以下开源项目：
- [Dear ImGui](https://github.com/ocornut/imgui) - 即时模式图形用户界面库
- [nanoid_cpp](https://github.com/mcmikecreations/nanoid_cpp) - C++ 实现的唯一 ID 生成器
- [GLFW](https://www.glfw.org/) - 跨平台窗口和输入处理库
- [Boost](https://www.boost.org/) - C++ 通用库集合

## 许可证

本项目使用的第三方库遵循各自的许可证：
- Dear ImGui: MIT License
- nanoid_cpp: MIT License
- GLFW: Zlib License
- Boost: Boost Software License