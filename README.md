# screen-share
# 屏幕共享软件客户端

基于 C++ / Qt 6 构建的局域网屏幕共享软件，支持多人在同一房间内共享屏幕、语音交互和实时标注。

## 功能特性

- 房间管理 — 创建/加入房间，成员实时同步
- 屏幕共享 — 一人共享，房间内所有人实时观看
- 抢共享机制 — 其他成员可请求接管共享权限
- 麦克风控制 — 开关麦克风（接口预留）
- 深色主题 UI — 现代化深色界面设计
- 跨平台 — 支持 Windows 和 macOS

## 项目结构

```
screen-share/
├── main.cpp                  # 程序入口
├── mainwindow.h/.cpp         # 主窗口，管理页面导航
├── pages/
│   ├── LoginPage.h/.cpp      # 登录页 — 输入昵称、房间号、服务器地址
│   └── RoomPage.h/.cpp       # 房间页 — 屏幕观看、工具栏、成员列表
├── widgets/
│   ├── ScreenView.h/.cpp     # 屏幕画面渲染组件
│   ├── MemberList.h/.cpp     # 成员列表组件
│   └── ToolButton.h/.cpp     # 工具栏按钮组件
├── network/
│   ├── RoomServer.h/.cpp     # TCP 房间管理服务器
│   └── RoomClient.h/.cpp     # TCP 房间管理客户端
├── .github/workflows/
│   └── build-mac.yml         # GitHub Actions 自动构建（macOS + Windows）
└── CMakeLists.txt            # CMake 构建配置
```

## 架构设计

客户端采用页面栈导航架构：

```
LoginPage → RoomPage
```

- **LoginPage** — 用户输入昵称和房间号，点击加入房间
- **RoomPage** — 主房间页面，包含屏幕共享显示区、控制工具栏、成员列表

通信流程：

```
客户端 A                        服务器 (TCP)                      客户端 B
  |                               |                               |
  |-- join (加入房间) ------------>|                               |
  |<-- member_list (成员列表) -----|                               |
  |                               |<-- join (加入房间) ------------|
  |<-- member_joined -------------|--> member_list --------------->|
  |                               |                               |
  |-- share_start (开始共享) ----->|                               |
  |<-- share_started (广播) ------|-- share_started (广播) ------->|
  |                               |                               |
  |-- share_stop (停止共享) ----->|                               |
  |<-- share_stopped (广播) ------|-- share_stopped (广播) ------->|
```

## 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| Qt | ≥ 6.5 | Core, Widgets, Network |
| CMake | ≥ 3.19 | 构建系统 |
| 编译器 | C++17 及以上 | Windows: MinGW / MSVC，macOS: Clang |

确保 `cmake`、`qmake`（或 Qt 的 `bin` 目录）已在系统 PATH 中。

## 编译与运行

### 方式一：命令行构建（Windows / macOS 通用）

```bash
# 1. 全量构建（首次或修改了 CMakeLists.txt 时）
cmake -B build
cmake --build build

# 2. 增量构建（仅修改了 .cpp/.h 文件时）
cmake --build build

# 3. 部署运行时依赖
#    Windows:
windeployqt build/screenShare.exe
#    macOS:
macdeployqt build/screenShare.app

# 4. 运行
#    Windows:
./build/screenShare.exe
#    macOS:
open build/screenShare.app
```

> 如果命令行找不到 `cmake` 或 `windeployqt`，需要将 Qt 和编译器的 `bin` 目录添加到 PATH。
> 例如（Windows + MinGW，路径根据实际安装位置调整）：
> ```bash
> export PATH="你的Qt路径/mingw_64/bin:你的MinGW路径/bin:$PATH"
> ```

### 方式二：Qt Creator

用 Qt Creator 打开项目根目录的 `CMakeLists.txt`，点击运行即可。

### 方式三：GitHub Actions 自动构建

推送到 GitHub 后自动编译，支持 macOS 和 Windows 双平台：

1. 将项目推送到 GitHub 仓库
2. 进入仓库 **Actions** 页面
3. 等待编译完成（约 3-5 分钟）
4. 在 Artifacts 区域下载对应平台的构建产物

也可手动点击 **Run workflow** 按钮触发构建。

## 技术栈

- **语言**: C++
- **UI 框架**: Qt 6 (Widgets)
- **实时通信**: WebRTC (libwebrtc)
- **信令通道**: 自定义 TCP 协议 (JSON)
- **构建系统**: CMake 3.19+
