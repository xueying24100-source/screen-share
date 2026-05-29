cmake_minimum_required(VERSION 3.21)
project(screen_share CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# 公共接口头文件（include/screen_share/...）
include_directories(${CMAKE_SOURCE_DIR}/include)

# ===== 客户端代码（跨平台共用） =====
file(GLOB_RECURSE CLIENT_SRC CONFIGURE_DEPENDS
    src/client/*.cpp
    src/client/*.h
    src/client/*.ui
)

# ===== 公共工具代码 =====
file(GLOB_RECURSE COMMON_SRC CONFIGURE_DEPENDS
    src/common/*.cpp
    src/common/*.h
)

# ===== 平台代码（按平台二选一） =====
if(WIN32)
    file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS
        src/platform/windows/*.cpp
        src/platform/windows/*.h
    )
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS
        src/platform/macos/*.mm
        src/platform/macos/*.cpp
        src/platform/macos/*.h
    )
endif()

qt_add_executable(screen_share
    ${CLIENT_SRC}
    ${COMMON_SRC}
    ${PLATFORM_SRC}
)

target_link_libraries(screen_share PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    # WGC / WinRT 等依赖按需在这里补
    # target_link_libraries(screen_share PRIVATE dwmapi gdi32 user32)
elseif(APPLE)
    # ScreenCaptureKit / AVFoundation 等
    # target_link_libraries(screen_share PRIVATE "-framework ScreenCaptureKit" "-framework AVFoundation")
endif()
