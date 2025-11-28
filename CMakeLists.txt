cmake_minimum_required(VERSION 3.10)
project(ConnectTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# Add ImGui
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PRIVATE glfw OpenGL::GL)

# Add nanoid_cpp
add_subdirectory(third_party/nanoid_cpp)

# TUN module sources (platform-specific)
set(TUN_SOURCES
    tun/tun_factory.cpp
)

if(UNIX AND NOT APPLE)
    # Linux
    list(APPEND TUN_SOURCES tun/tun_linux.cpp)
    message(STATUS "Building TUN module for Linux")
elseif(APPLE)
    # macOS
    list(APPEND TUN_SOURCES tun/tun_macos.cpp)
    message(STATUS "Building TUN module for macOS")
elseif(WIN32)
    # Windows
    list(APPEND TUN_SOURCES tun/tun_windows.cpp)
    message(STATUS "Building TUN module for Windows (Wintun)")
    include_directories(${CMAKE_SOURCE_DIR}/third_party/wintun/api)
endif()

# Add Online Game Tool executable
add_executable(OnlineGameTool 
    online_game_tool.cpp 
    steam/steam_message_handler.cpp 
    steam/steam_networking_manager.cpp 
    steam/steam_room_manager.cpp 
    steam/steam_utils.cpp 
    steam/steam_vpn_bridge.cpp
    ${TUN_SOURCES}
)

# Set compile options for UTF-8 support
if(MSVC)
    target_compile_options(OnlineGameTool PRIVATE /utf-8)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/steam)
include_directories(${CMAKE_SOURCE_DIR}/third_party/nanoid_cpp/inc)
include_directories(${CMAKE_SOURCE_DIR}/tun)

# Include Steamworks
include_directories(${CMAKE_SOURCE_DIR}/third_party/steamsdk/public/steam)

# Link libraries
target_link_libraries(OnlineGameTool PRIVATE imgui nanoid ${CMAKE_SOURCE_DIR}/third_party/steamsdk/redistributable_bin/win64/steam_api64.lib ws2_32 glfw)

# Add TUN example executable (optional)
option(BUILD_TUN_EXAMPLE "Build TUN example program" ON)
if(BUILD_TUN_EXAMPLE)
    add_executable(TunExample tun/example_tun.cpp ${TUN_SOURCES})
    
    # Platform-specific libraries for TUN example
    if(WIN32)
        target_link_libraries(TunExample PRIVATE iphlpapi ws2_32 ole32)
    elseif(UNIX)
        target_link_libraries(TunExample PRIVATE pthread)
    endif()
    
    message(STATUS "TUN example will be built")
endif()