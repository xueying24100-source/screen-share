# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

局域网屏幕共享软件客户端，基于 C++ / Qt6 / WebRTC 构建。支持多人在同一房间内共享屏幕、语音交互、画中画和标注画笔功能。

## 构建与运行

### 环境要求

- Qt 6.10.3（安装路径 `D:/Qt/6.10.3/mingw_64`）
- MinGW 13.1.0 64-bit（安装路径 `D:/Qt/Tools/mingw1310_64`）
- CMake（安装路径 `D:/Qt/Tools/CMake_64`）

### 方式一：命令行构建

在项目根目录 `D:\QTCode\screen-share` 下打开终端，执行：

```bash
# 0. 设置环境变量（每次开新终端都需要执行）
export PATH="/d/Qt/Tools/CMake_64/bin:/d/Qt/6.10.3/mingw_64/bin:/d/Qt/Tools/mingw1310_64/bin:$PATH"

# 1. 全量构建（首次或修改了 CMakeLists.txt 时使用）
rm -rf build
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 2. 增量构建（仅修改了 .cpp/.h 文件时使用，更快）
cmake --build build

# 3. 部署 DLL（首次构建或 Qt 版本变更后需要执行，让 exe 可双击运行）
windeployqt build/screenShare.exe

# 4. 运行
./build/screenShare.exe
```

### 方式二：Qt Creator

直接用 Qt Creator 打开项目根目录的 `CMakeLists.txt`，点击运行即可。

### 注意事项

- 步骤 3（windeployqt）会将 Qt6Core.dll、Qt6Widgets.dll、Qt6Network.dll 等依赖复制到 `build/` 目录，这样双击 exe 就能运行
- 如果只修改了 .cpp/.h 文件，只需执行步骤 2 即可，不需要重新配置和部署
- 如果修改了 `CMakeLists.txt`（比如添加了新源文件），需要从步骤 1 开始全量构建

## 技术栈

- **语言**: C++
- **UI 框架**: Qt 6 (Widgets)
- **实时通信**: WebRTC (libwebrtc)
- **信令**: 自定义 WebSocket 信令通道
- **构建系统**: CMake 3.19+

## 团队分工

- **蒋宗原**: 屏幕共享模块 - 窗口枚举与采集 (Windows)
- **韦燕丹**: 屏幕共享模块 - 屏幕枚举与采集 (Windows)
- **俞哲钊**: 屏幕共享模块 - 窗口枚举与采集 (macOS)
- **邢雨茁**: 屏幕共享模块 - 屏幕枚举与采集 (macOS)
- **黄俊杰**: 客户端业务架构 (macOS) — UI 页面、房间管理、WebRTC 集成
- **赵芃年**: 客户端业务架构 (Windows)

#  我负责的模块

**黄俊杰**: 客户端业务架构 (macOS) — UI 页面、房间管理

## 架构设计

客户端采用页面栈导航架构：

```
LoginPage → RoomPage
```

- **LoginPage**: 用户输入昵称和房间号，点击加入房间
- **RoomPage**: 主房间页面，包含屏幕共享显示区、控制工具栏（共享/停止/抢共享/麦克风/退出）、成员列表、状态信息

WebRTC 层通过信令通道交换 SDP/ICE，建立 P2P 连接后传输屏幕流和音频流。

## 代码规范

- 所有文档和注释使用中文编写
- 修改代码前需先与用户描述修改思路，确认后再进行修改
- 创建总结性 .md 文件时遵循 Obsidian 文档编写规范
