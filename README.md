# screen-share macOS 客户端

基于 C++ / Qt 6 / Objective-C++ 的局域网屏幕共享客户端。当前 `macOS` 分支合并了客户端业务框架和 macOS ScreenCaptureKit 采集模块，支持房间状态流转、本机真实屏幕/窗口采集预览、本机双客户端同步采集演示、画笔标注同步和麦克风 PCM 采集。

## 当前能力

- 登录/加入房间：输入昵称、房间号、服务器地址和端口。
- 房间成员列表：同步成员加入、离开和共享状态。
- 共享状态信令：支持开始共享、停止共享、抢共享请求与响应。
- macOS 来源选择：开始共享时弹出屏幕/窗口来源选择列表。
- 本机真实采集预览：共享者本机通过 `ScreenCaptureManager` 采集并显示到 `ScreenView`。
- 本机双客户端同步采集：共享者选择来源后，观看端会收到 `sourceId/sourceType`，并在同一台 Mac 上自行采集同一个屏幕/窗口来源。
- 画笔标注同步：本地画笔使用归一化坐标绘制，并通过 TCP JSON 信令同步给同房间其他客户端。
- 麦克风采集：点击麦克风后使用 Qt Multimedia 采集本机默认麦克风 PCM 数据。

当前限制：

- TCP 网络层目前负责房间状态、共享来源信息和画笔标注命令。
- 双客户端真实画面演示依赖同一台 Mac 上的同步采集；真实视频帧尚未跨设备传输到另一个客户端。
- 麦克风目前只完成本机采集，尚未通过网络发送给远端播放。
- 系统音频采集当前默认关闭，尚未接入远端播放。

## 项目结构

```text
screen-share/
├── main.cpp
├── mainwindow.h / mainwindow.cpp
├── pages/
│   ├── LoginPage.h / LoginPage.cpp
│   └── RoomPage.h / RoomPage.cpp
├── widgets/
│   ├── ScreenView.h / ScreenView.cpp
│   ├── MemberList.h / MemberList.cpp
│   └── ToolButton.h / ToolButton.cpp
├── network/
│   ├── RoomServer.h / RoomServer.cpp
│   └── RoomClient.h / RoomClient.cpp
├── include/screen_share/
│   ├── ScreenCaptureManager.h
│   └── AnnotationTypes.h
├── src/macos/
│   └── ScreenCaptureManager.mm
├── docs/                                 # 接口说明、协议说明和客户端规划文档
├── CMakeLists.txt
└── Info.plist
```

## 核心模块

`MainWindow`

管理 `LoginPage` 和 `RoomPage` 页面切换，创建并连接 `RoomClient` / `RoomServer`。

`RoomPage`

房间主界面，负责共享按钮、抢共享、麦克风、画笔、来源选择、本机采集预览和观看端同步采集接入。

`ScreenView`

屏幕画面渲染控件。支持真实帧渲染、模拟观看画面、画笔 overlay、远端画笔点位重放、停止共享后的内容清理。

`RoomClient` / `RoomServer`

基于 TCP + JSON 的房间信令。当前处理 `join`、`leave`、`share_start`、`share_stop`、`grab_share`、`grab_respond`、`annotation` 等消息。

`ScreenCaptureManager`

macOS 采集封装。使用 ScreenCaptureKit 枚举屏幕/窗口来源，启动 `SCStream`，输出视频帧和可选系统音频数据。

## macOS 采集链路

```text
RoomPage
-> selectCaptureSource()
-> RoomClient::requestShareStart(sourceId, sourceType)
-> RoomServer broadcast share_started
-> ScreenCaptureManager::enumerateDisplays() / enumerateWindows()
-> ScreenCaptureManager::startCapture(id, type)
-> ScreenCaptureKit SCStream
-> frameCaptured(...)
-> ScreenView::updateFrame(frame)
```

停止共享时：

```text
RoomPage::stopLocalCapturePreview()
-> ScreenCaptureManager::stopAllCaptures()
-> ScreenView::clearContent()
```

## 构建环境

| 依赖 | 版本/说明 |
|------|-----------|
| Qt | 6.5 或以上，需包含 Core、Widgets、Network、Multimedia |
| CMake | 3.19 或以上 |
| macOS | 13.0 或以上 |
| 编译器 | Apple Clang，支持 C++17 / Objective-C++ |

## 构建与运行

如果 Qt 不在默认 CMake 搜索路径中，需要指定 `CMAKE_PREFIX_PATH`：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/Users/yuzhuo/Qt/6.11.1/macos
cmake --build build
open build/screenShare.app
```

如果 Qt 已在环境变量中：

```bash
cmake -B build
cmake --build build
open build/screenShare.app
```

## 权限说明

真实屏幕/窗口采集需要 macOS 录屏权限：

```text
系统设置 -> 隐私与安全性 -> 录屏与系统录音
```

麦克风采集需要 macOS 麦克风权限。

授权后通常需要完全退出 app，再重新打开。

## 演示流程

1. 启动 app。
2. 输入昵称、房间号、服务器地址 `127.0.0.1` 和端口。
3. 加入房间。
4. 点击 `共享屏幕`。
5. 在弹窗中选择屏幕或窗口来源。
6. 点击 `开始共享`，共享者显示真实采集预览。
7. 如果同一台 Mac 上已加入第二个客户端，观看端会按相同来源自行采集并显示真实画面。
8. 点击 `画笔` 后在画面上拖拽绘制标注，另一个客户端会同步显示画笔痕迹。
9. 点击 `清空标注` 清除画笔内容，并同步清空另一个客户端的标注。
10. 点击 `麦克风` 开始采集本机麦克风 PCM。
11. 点击 `停止共享` 后，画面会恢复为等待共享占位状态。

## 双客户端演示

同一台电脑可以尝试用不同 app bundle 启动两个客户端。最稳定做法是复制一份 app 并修改 bundle id：

```bash
ditto build/screenShare.app /private/tmp/screenShare2.app
plutil -replace CFBundleIdentifier -string com.yuzhuo.screen-share.second /private/tmp/screenShare2.app/Contents/Info.plist
plutil -replace CFBundleName -string screenShare2 /private/tmp/screenShare2.app/Contents/Info.plist
plutil -replace CFBundleDisplayName -string screenShare2 /private/tmp/screenShare2.app/Contents/Info.plist
open -n build/screenShare.app
open -n /private/tmp/screenShare2.app
```

两个客户端使用不同昵称，加入同一个房间号即可演示成员同步、共享状态、同源采集画面和画笔标注同步。

注意：双客户端真实画面依赖两个客户端都运行在同一台 Mac 上。观看端不是接收共享者的视频帧，而是收到共享来源信息后自行采集同一个本机屏幕/窗口；跨设备真实共享仍需要后续接入 WebRTC 或其他媒体传输方案。

## 后续工作

- 接入 WebRTC，将本机采集的视频帧传输到远端客户端。
- 接入远端音频播放，让麦克风 PCM 能被其他成员听到。
- 将画笔标注命令从 TCP 信令迁移到 WebRTC DataChannel。
- 完善窗口来源筛选策略和权限提示。
- 将系统音频采集与麦克风通话能力区分展示。
