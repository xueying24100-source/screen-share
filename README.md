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
│   ├── ScreenView.h/.cpp     # 屏幕画面渲染 + 标注绘制
│   ├── MemberList.h/.cpp     # 成员列表组件
│   └── ToolButton.h/.cpp     # 工具栏按钮组件
├── network/
│   ├── RoomServer.h/.cpp     # TCP 房间管理服务器
│   └── RoomClient.h/.cpp     # TCP 房间管理客户端
├── capture/
│   ├── screencapturer.h      # 采集模块公共头文件
│   ├── screencapturer.cpp    # 采集模块 Windows 实现
│   ├── screencapturer.mm     # 采集模块 macOS 实现（SCStream + CoreGraphics）
│   ├── sourceenumerator.h/.cpp  # 屏幕/窗口枚举
│   ├── sharesourcepicker.h/.cpp # 共享源选择对话框
│   └── AnnotationTypes.h     # 标注数据类型定义
├── .github/workflows/
│   └── build-mac.yml         # GitHub Actions 自动构建（macOS + Windows）
└── CMakeLists.txt            # CMake 构建配置
```

## 核心模块与方法索引

### 屏幕采集（macOS: SCStream 流式推流）

| 方法 | 位置 | 说明 |
|------|------|------|
| `ScreenCapturer::startScreen()` | `capture/screencapturer.h:26` | 设置屏幕模式并启动采集 |
| `ScreenCapturer::start()` | `capture/screencapturer.mm` | macOS 屏幕模式走 SCStream，降级走 QTimer |
| `SCStreamFrameBridge` | `capture/screencapturer.mm` | SCStream 回调桥接，CVPixelBuffer → QImage |
| `startScreenStream()` | `capture/screencapturer.mm` | 创建 SCStream 并启动流式采集 |
| `stopScreenStream()` | `capture/screencapturer.mm` | 停止 SCStream 并释放资源 |
| `qImageFromPixelBuffer()` | `capture/screencapturer.mm` | CVPixelBuffer 转 QImage |
| `copyShareableContentSync()` | `capture/screencapturer.mm` | 同步获取 SCShareableContent |

### 屏幕采集（Windows / macOS 降级）

| 方法 | 位置 | 说明 |
|------|------|------|
| `ScreenCapturer::captureScreen()` | `capture/screencapturer.cpp` / `.mm` | QTimer 定时器触发，QScreen::grabWindow 截图 |
| `ScreenCapturer::stop()` | `capture/screencapturer.h:28` | 停止采集（SCStream 或 QTimer） |

### 窗口采集（macOS: ScreenCaptureKit + CoreGraphics 双后端）

| 方法 | 位置 | 说明 |
|------|------|------|
| `ScreenCapturer::startWindow()` | `capture/screencapturer.h:27` | 设置窗口模式并启动采集 |
| `captureWindow()` | `capture/screencapturer.mm` | 调用 captureWindowRaw 获取帧 |
| `captureWindowRaw()` | `capture/screencapturer.mm` | 优先 ScreenCaptureKit，降级 CoreGraphics |
| `captureWindowRawWithScreenCaptureKit()` | `capture/screencapturer.mm` | macOS 14+ 高清窗口截图 |
| `captureWindowRawWithCoreGraphics()` | `capture/screencapturer.mm` | CGWindowListCreateImage 回退方案 |
| `captureWindowOnce()` | `capture/screencapturer.h:34` | 静态方法，一次性截图（用于缩略图预览） |

### 屏幕/窗口枚举与选择

| 方法 | 位置 | 说明 |
|------|------|------|
| `SourceEnumerator::enumerateScreens()` | `capture/sourceenumerator.h` | 枚举所有显示器 |
| `SourceEnumerator::enumerateWindows()` | `capture/sourceenumerator.h` | 枚举所有可见窗口 |
| `ShareSourcePicker` | `capture/sharesourcepicker.h/.cpp` | 共享源选择对话框（卡片式 UI） |

### 画笔标注模块

| 类型 | 位置 | 说明 |
|------|------|------|
| `AnnotationTool` / `AnnotationAction` | `capture/AnnotationTypes.h` | 标注工具（画笔/矩形）和动作枚举 |
| `AnnotationCommand` | `capture/AnnotationTypes.h` | 标注命令结构体（归一化坐标，用于网络同步） |
| `ScreenView::setAnnotationTool()` | `widgets/ScreenView.h` | 设置标注工具类型 |
| `ScreenView::addAnnotation()` | `widgets/ScreenView.h` | 添加远程标注命令并渲染 |
| `ScreenView::clearAnnotations()` | `widgets/ScreenView.h` | 清除所有标注 |
| `ScreenView::annotationCreated()` | `widgets/ScreenView.h` | 信号，本地标注完成时发射 |
| `RoomClient::sendAnnotation()` | `network/RoomClient.h` | 发送标注命令到服务器 |
| `RoomServer::handleMessage()` | `network/RoomServer.cpp:128` | 转发 annotation 消息到房间成员 |

### 帧传输与渲染

| 方法 | 位置 | 说明 |
|------|------|------|
| `ScreenCapturer::frameCaptured()` | `capture/screencapturer.h` | 信号，每帧采集完成时发射 |
| `RoomPage::onLocalFrameCaptured()` | `pages/RoomPage.cpp` | 本地帧预览 + JPEG 编码发送 |
| `RoomPage::onRemoteFrameReceived()` | `pages/RoomPage.cpp` | 接收远程 JPEG 帧并解码渲染 |
| `RoomClient::sendVideoFrame()` | `network/RoomClient.h` | 发送视频帧到服务器 |
| `ScreenView::updateFrame()` | `widgets/ScreenView.h` | 更新显示的画面帧 |

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

---

## 屏幕共享模块对接指南

本章节描述客户端业务架构（已完成）与屏幕共享模块（窗口枚举与采集、屏幕枚举与采集）的对接流程。

### 模块分工

| 模块 | 负责人 | 平台 | 职责 |
|------|--------|------|------|
| 窗口枚举与采集 | 蒋宗原 | Windows | 枚举系统窗口列表，采集指定窗口画面 |
| 屏幕枚举与采集 | 韦燕丹 | Windows | 枚举显示器列表，采集指定屏幕画面 |
| 窗口枚举与采集 | 俞哲钊 | macOS | 枚举系统窗口列表，采集指定窗口画面 |
| 屏幕枚举与采集 | 邢雨茁 | macOS | 枚举显示器列表，采集指定屏幕画面 |
| 客户端业务架构 | 黄俊杰 / 赵芃年 | macOS / Windows | UI 页面、房间管理、WebRTC 集成 |

### 采集模块预期接口

屏幕采集模块应封装为一个 `ScreenCapturer` 类，提供以下接口供客户端业务架构层调用：

```cpp
// 预期的采集模块接口（采集模块开发者需实现此接口）
class ScreenCapturer : public QObject {
    Q_OBJECT
public:
    // 采集源信息
    struct CaptureSource {
        int id;           // 源的唯一标识
        QString title;    // 窗口标题或显示器名称
        bool isScreen;    // true=屏幕, false=窗口
    };

    // 获取可用的屏幕/窗口列表
    QList<CaptureSource> getSourceList();

    // 选择采集源（通过 getSourceList 返回的 id）
    bool selectSource(int sourceId);

    // 开始采集，fps 为目标帧率（建议 10-15）
    bool startCapture(int fps = 15);

    // 停止采集
    void stopCapture();

    // 当前是否正在采集
    bool isCapturing() const;

signals:
    // 每帧采集完成时发射，frame 为 QImage 格式
    void frameCaptured(const QImage &frame);
};
```

**接口约定**：

- `frameCaptured` 信号的 `QImage` 像素格式应为 `QImage::Format_ARGB32` 或 `QImage::Format_RGB32`（Qt 标准格式）
- macOS 采集模块需要注意 `CGDisplayCreateImage` 返回的 BGRA 数据需要转换为 ARGB 格式
- 采集操作应在独立线程执行，`frameCaptured` 信号通过 `Qt::QueuedConnection` 传递到 UI 线程
- macOS 需要"屏幕录制权限"，采集模块应在无法采集时返回明确错误信息

### 对接流程

#### 第一步：集成采集模块源文件

采集模块开发者将源文件放入项目中，建议目录结构：

```
screen-share/
├── capture/                         # 新建采集模块目录
│   ├── ScreenCapturer.h             # 采集模块头文件（接口定义）
│   ├── ScreenCapturer.cpp           # 采集模块实现（平台相关）
│   ├── WindowEnumerator.h/.cpp      # 窗口枚举（可选，按模块内部设计）
│   └── ScreenEnumerator.h/.cpp      # 屏幕枚举（可选，按模块内部设计）
```

#### 第二步：修改 CMakeLists.txt

在 `qt_add_executable` 中添加采集模块源文件，并增加 include 路径：

```cmake
qt_add_executable(screenShare
    # ... 现有文件 ...
    capture/ScreenCapturer.cpp
    capture/ScreenCapturer.h
)

target_include_directories(screenShare PRIVATE
    # ... 现有路径 ...
    ${CMAKE_CURRENT_SOURCE_DIR}/capture
)
```

#### 第三步：在 RoomPage 中集成采集模块

修改 `pages/RoomPage.h`，添加采集模块成员：

```cpp
#include "ScreenCapturer.h"

class RoomPage : public QWidget {
    // ...
private:
    ScreenCapturer *m_capturer = nullptr;
};
```

修改 `pages/RoomPage.cpp`，在构造函数中初始化并连接信号：

```cpp
m_capturer = new ScreenCapturer(this);

// 采集帧 → ScreenView 渲染
connect(m_capturer, &ScreenCapturer::frameCaptured,
        m_screenView, &ScreenView::updateFrame);
```

#### 第四步：修改共享按钮逻辑

修改 `RoomPage::onShareClicked()`，将模拟逻辑替换为真实采集调用：

```cpp
void RoomPage::onShareClicked() {
    if (!m_isSharing) {
        // 开始采集
        if (m_capturer->startCapture(15)) {
            m_isSharing = true;
            setSharingUI(true);
        }
    } else {
        // 停止采集
        m_capturer->stopCapture();
        m_isSharing = false;
        setSharingUI(false);
    }
}
```

#### 第五步：ScreenView 帧渲染

`widgets/ScreenView.h` 已预留 `updateFrame(const QImage &frame)` 接口，采集模块的 `frameCaptured` 信号可直接连接到此方法。渲染逻辑在 `paintEvent` 中自动处理缩放和居中显示。

### 对接流程图

```
采集模块开发者                          客户端业务架构
    │                                      │
    │  1. 提供 ScreenCapturer 头文件和实现   │
    │──────────────────────────────────────>│
    │                                      │  2. 放入 capture/ 目录
    │                                      │  3. 修改 CMakeLists.txt
    │                                      │  4. RoomPage 中创建实例
    │                                      │  5. 连接 frameCaptured → updateFrame
    │                                      │  6. 连接 共享按钮 → startCapture/stopCapture
    │                                      │
    │                                      │  7. 编译联调
    │  8. 确认帧格式、分辨率、帧率           │
    │<─────────────────────────────────────>│
    │                                      │  9. 验证渲染效果
    │                                      │
    │  10. WebRTC 推流集成（后续阶段）       │
    │<─────────────────────────────────────>│
```

### 联调检查清单

对接前需双方确认以下事项：

- [ ] 确认采集模块头文件路径和类名
- [ ] 确认 `frameCaptured` 信号参数格式（`QImage` 还是原始数据 + 宽高）
- [ ] 确认像素格式（BGRA / ARGB / RGB），用于颜色转换
- [ ] 确认 macOS 屏幕录制权限的检查和引导方式
- [ ] 确认采集模块是否线程安全（采集线程 vs UI 线程）
- [ ] 在 `CMakeLists.txt` 中添加采集模块源文件和 include 路径
- [ ] 编译联调，验证帧能正确渲染到 ScreenView
- [ ] 确认采集帧率和分辨率参数（建议 10-15 fps，1280x720 或 1920x1080）

### macOS 特殊说明

macOS 平台对接时需注意：

1. **屏幕录制权限**：macOS 10.15+ 需要在"系统偏好设置 → 安全性与隐私 → 屏幕录制"中授权应用。采集模块应在 `startCapture()` 失败时提供明确的权限缺失提示
2. **像素格式**：macOS 的 `CGDisplayCreateImage` 返回 BGRA 格式，需转换为 Qt 支持的 ARGB 格式（或使用 `QImage::Format_RGB32`）
3. **Retina 显示器**：macOS Retina 屏幕的实际像素尺寸是逻辑尺寸的 2 倍，采集时需注意 `backingScaleFactor` 的处理
4. **编译器**：macOS 使用 Clang（Xcode Command Line Tools），确保 C++17 特性可用
