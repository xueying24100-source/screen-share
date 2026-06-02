# macOS 窗口采集 Demo

这是 `yzz` 分支的 macOS 窗口枚举与采集模块 demo。它对齐 Windows 端采集分支的接口风格，目标是验证 macOS 端可以完成：

- 枚举当前可共享的屏幕和应用窗口
- 选择一个屏幕或窗口作为采集源
- 将选中的源连续采集为 `QImage` 帧
- 在本地 demo 窗口中实时预览采集结果

注意：这个目录不是正式房间客户端，不包含登录、房间、成员列表、音频、编码、网络传输或接收端显示逻辑。它是一个独立测试程序，用来交付和验证 macOS 采集模块本身。

## 已完成内容

- `SourceEnumerator::enumerateScreens()`：枚举 macOS 当前屏幕。
- `SourceEnumerator::enumerateWindows()`：枚举 macOS 当前可见应用窗口。
- `ScreenCapturer::startScreen()`：按屏幕序号开始屏幕采集。
- `ScreenCapturer::startWindow()`：按窗口 ID 开始窗口采集。
- `ScreenCapturer::stop()`：停止定时采集。
- `ScreenCapturer::captureWindowOnce()`：对指定窗口进行一次性截图，可用于窗口缩略图。
- `ShareSourcePicker`：选择共享内容弹窗，支持屏幕/窗口和帧率选择。
- `main.cpp`：独立 demo 主窗口，负责启动采集、预览帧、停止采集。

## 目录结构

```text
mac-window-capture-yzz/
├── CMakeLists.txt
├── README.md
├── main.cpp                  # 独立测试窗口：选择目标、预览采集帧、停止采集
├── sourceenumerator.h/.cpp   # 屏幕/窗口枚举
├── screencapturer.h/.cpp     # 屏幕/窗口采集，输出 QImage
└── sharesourcepicker.h/.cpp  # 选择共享内容弹窗
```

## 核心接口

后续接入正式客户端时，主要复用下面这些接口：

```cpp
QList<ScreenInfo> SourceEnumerator::enumerateScreens();
QList<WindowInfo> SourceEnumerator::enumerateWindows();

void ScreenCapturer::startScreen(int screenIndex, int fps = 15);
void ScreenCapturer::startWindow(quintptr windowId, int fps = 15);
void ScreenCapturer::stop();

static QImage ScreenCapturer::captureWindowOnce(
    quintptr windowId,
    const QSize &outputSize = {});
```

采集结果通过 Qt 信号输出：

```cpp
void frameCaptured(const QImage &frame);
void captureError(const QString &msg);
void frameMetadataChanged(const CaptureFrameMetadata &meta);
```

## 技术方案

当前版本使用 CoreGraphics + Qt：

- 屏幕枚举：`QGuiApplication::screens()`
- 屏幕采集：`QScreen::grabWindow(0)`
- 窗口枚举：`CGWindowListCopyWindowInfo`
- 窗口采集：`CGWindowListCreateImage`
- 图像输出：转换为 `QImage::Format_RGB32`
- 定时采集：`QTimer`

窗口实时采集和窗口缩略图共用 `captureWindowOnce()` 的单帧截图逻辑，避免两套采集实现分叉。

macOS 14 开始 `CGWindowListCreateImage` 会提示 deprecated。这个版本适合课程 demo 和接口联调；后续正式版本可以在保持上层接口不变的情况下，把底层替换为 `ScreenCaptureKit`。

## 环境要求

- macOS
- Qt 6.5 或更高版本，需包含 Core / Gui / Widgets
- CMake
- C++17 编译器

本机验证环境：

- Qt 6.11.1
- Qt 自带 CMake
- AppleClang / Command Line Tools

## 构建与运行

先切到 `yzz` 分支并进入 demo 目录：

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

如果 Qt 或 CMake 路径不同，请把上面的路径换成自己的安装路径。也可以用 Qt Creator 打开本目录下的 `CMakeLists.txt` 构建运行。

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

```text
系统设置 -> 隐私与安全性 -> 屏幕与系统音频录制
```

给 `mac_window_capture_yzz` 或当前运行的 demo app 打开权限，然后完全退出 demo 并重新打开。

## 与 Windows 端对齐

Windows 端 `jzy` 分支有 `captureWindowOnce()` 用于窗口缩略图和单次截图。本 demo 已补齐同类接口：

```cpp
ScreenCapturer::captureWindowOnce(windowId, QSize(220, 124));
```

区别在于底层实现不同：

- Windows：`PrintWindow` / `BitBlt` / 屏幕裁剪
- macOS：`CGWindowListCreateImage`

上层调用方式保持相近，方便后续合并到统一客户端。

## 后续集成方式

正式接入 `hjj` 客户端时，可以这样连接采集帧到显示组件：

```cpp
connect(capturer, &ScreenCapturer::frameCaptured,
        screenView, &ScreenView::updateFrame);
```

选择窗口后：

```cpp
capturer->startWindow(windowId, 15);
```

选择屏幕后：

```cpp
capturer->startScreen(screenIndex, 15);
```

停止共享时：

```cpp
capturer->stop();
```

窗口选择弹窗可以复用 `ShareSourcePicker`，也可以只复用 `SourceEnumerator` 和 `ScreenCapturer` 接到客户端自己的 UI。

## 已知限制

- 当前窗口采集使用 `CGWindowListCreateImage`，在 macOS 14 SDK 下会出现 deprecated warning。
- 该 demo 只负责本地采集和本地预览，不包含编码、网络发送、接收端显示。
- 部分受保护内容、最小化窗口或系统限制窗口可能无法采集。
- 窗口缩略图如果截图失败，会回退为占位图。
- 屏幕共享采集自己窗口时会出现递归画面，这是屏幕采集 demo 的正常现象。

## 汇报要点

可以这样概括本模块：

> 我负责 macOS 端窗口枚举与画面采集。当前完成了独立 demo，能够枚举屏幕和应用窗口，选择采集源后定时输出 `QImage` 帧并本地预览；接口对齐 Windows 端的 `startWindow`、`startScreen`、`captureWindowOnce`，后续可以接入客户端显示或网络发送模块。
