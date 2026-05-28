# macOS 窗口采集 Demo

这是 `yzz` 分支的 macOS 窗口枚举与采集 demo。

注意：这个目录不是正式房间客户端，不包含登录、房间、成员列表、网络传输等业务逻辑。它是一个独立测试程序，用来验证 macOS 端能否完成“列出可共享窗口/屏幕，并把选中的目标采集成连续 `QImage` 帧”。

后续和客户端集成时，主要复用这里的采集接口和实现：

- `SourceEnumerator::enumerateScreens()`
- `SourceEnumerator::enumerateWindows()`
- `ScreenCapturer::startScreen()`
- `ScreenCapturer::startWindow()`
- `ScreenCapturer::stop()`
- `frameCaptured(const QImage&)`
- `captureError(const QString&)`
- `frameMetadataChanged(const CaptureFrameMetadata&)`

当前版本使用 CoreGraphics：

- `CGWindowListCopyWindowInfo` 枚举窗口
- `CGWindowListCreateImage` 采集指定窗口
- `QScreen::grabWindow(0)` 采集屏幕

macOS 14 开始 `CGWindowListCreateImage` 会提示 deprecated。它适合课程 demo 和接口联调，后续正式版本可以在不改上层接口的前提下替换为 `ScreenCaptureKit`。

## 目录结构

```text
mac-window-capture-yzz/
├── CMakeLists.txt
├── main.cpp                  # 独立测试窗口：选择目标、预览采集帧、停止采集
├── sourceenumerator.h/.cpp   # 屏幕/窗口枚举
├── screencapturer.h/.cpp     # 屏幕/窗口采集，输出 QImage
└── sharesourcepicker.h/.cpp  # 选择共享内容弹窗
```

## 环境要求

- macOS
- Qt 6.5 或更高版本，已安装 Core / Gui / Widgets
- CMake
- C++17 编译器

本机验证环境：

- Qt 6.11.1
- Qt 自带 CMake
- AppleClang / Command Line Tools

## 构建与运行

先切到你的分支并进入 demo 目录：

```bash
git fetch origin
git checkout yzz
cd mac-window-capture-yzz
```

如果 Qt 安装在 `/Users/<你的用户名>/Qt/6.11.1/macos`，可以这样构建：

```bash
/Users/<你的用户名>/Qt/Tools/CMake/CMake.app/Contents/bin/cmake -B build -DCMAKE_PREFIX_PATH=/Users/<你的用户名>/Qt/6.11.1/macos
/Users/<你的用户名>/Qt/Tools/CMake/CMake.app/Contents/bin/cmake --build build
open build/mac_window_capture_yzz.app
```

如果你的 Qt 或 CMake 路径不同，请把上面的路径换成自己的安装路径。

## 使用流程

1. 打开 demo。
2. 点击 `选择并开始采集`。
3. 在弹窗里选择 `整个屏幕` 或 `应用窗口`。
4. 选择帧率。
5. 点击 `开始采集`。
6. 主窗口中间区域会显示实时采集画面。
7. 点击 `停止采集` 后，预览区会清空并回到等待状态。

## 权限说明

macOS 采集屏幕或窗口通常需要屏幕录制权限。如果窗口列表正常但采集不到画面，或者画面为空，请打开：

`系统设置 -> 隐私与安全性 -> 屏幕与系统音频录制`

给 `mac_window_capture_yzz` 或当前运行的 demo app 打开权限，然后完全退出 demo 并重新打开。

## 已知限制

- 当前窗口采集使用 `CGWindowListCreateImage`，在 macOS 14 SDK 下会出现 deprecated warning。
- 该 demo 只负责本地采集和本地预览，不包含编码、网络发送、接收端显示。
- 部分受保护内容、最小化窗口或系统限制窗口可能无法采集。
- 窗口缩略图目前使用占位图，实际画面会在开始采集后显示。

## 后续集成方式

正式接入客户端时，客户端可以这样连接：

```cpp
connect(capturer, &ScreenCapturer::frameCaptured,
        screenView, &ScreenView::updateFrame);
```

窗口选择后调用：

```cpp
capturer->startWindow(windowId, 15);
```

屏幕选择后调用：

```cpp
capturer->startScreen(screenIndex, 15);
```

停止共享时调用：

```cpp
capturer->stop();
```
