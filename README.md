# Screen Share

基于 Qt6 + C++20 的桌面屏幕共享系统，支持屏幕/窗口采集、实时音频传输与多人协同标注。

---

## 功能概览

| 功能模块 | 描述 |
|---------|------|
| 🖥️ 屏幕采集 | 支持全屏/指定屏幕/窗口三种模式，Windows 下优先使用 DXGI/WGC 高性能后端，自动降级到 GrabWindow |
| 🎙️ 语音通话 | 麦克风采集 PCM（16kHz/单声道/16-bit），扬声器实时播放远端音频 |
| ✏️ 协同标注 | 透明绘图层覆盖在共享画面上，支持多人实时笔划同步与文字标注 |
| 📡 多流发送 | Sender 模块汇聚视频/音频/标注流，按优先级队列调度发送，编码器与传输层均可替换 |

---

## 项目结构

```
screen-share/
├── main.cpp                    # 程序入口
├── mainwindow.cpp/h            # 主窗口
│
├── screencapturer.cpp/h        # 屏幕采集模块（主模块）
├── wgcwindowcapturebackend.cpp/h  # Windows WGC 窗口捕获后端
├── sourceenumerator.cpp/h      # 屏幕/窗口枚举
│
├── audiocapturer.cpp/h         # 麦克风采集
├── audioplayer.cpp/h           # 音频播放
│
├── annotationoverlay.cpp/h     # 画笔标注叠加层
├── annotationwindow.cpp/h      # 标注工具窗口
│
├── sender.cpp/h                # 多路媒体发送调度中心
│
├── CMakeLists.txt              # 构建配置
│
├── DESIGN.md                   # 屏幕采集模块设计文档
├── AUDIO_DESIGN.md             # 语音模块设计文档
├── ANNOTATION_DESIGN.md        # 标注模块设计文档
└── SENDER_DESIGN.md            # Sender 模块设计文档
```

---

## 环境要求

| 依赖 | 版本 |
|------|------|
| Qt | 6.x（Core / Gui / Widgets / Multimedia）|
| CMake | ≥ 3.16 |
| 编译器 | C++20 支持（MSVC 2022 / GCC 12+ / Clang 14+）|
| 操作系统 | Windows 10/11（完整功能）；macOS / Linux（仅 GrabWindow 路径）|

> Windows 下需链接 `d3d11`、`dxgi`、`user32`、`WindowsApp`，CMakeLists.txt 已自动配置。

---

## 构建步骤

```bash
# 克隆仓库
git clone https://github.com/xueying24100-source/screen-share.git
cd screen-share

# 切换到目标分支（如 wyd）
git checkout wyd

# 配置并构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

构建产物位于 `build/` 目录下，可执行文件名为 `ScreenShare_Capturer`。

---

## 模块说明

### ScreenCapturer — 屏幕采集

- 输出信号：`frameCaptured(const QImage&)`，30fps，分辨率缩放至 1280×720
- Windows：优先走 **DXGI Desktop Duplication**（可截取游戏/视频），失败自动降级
- 支持 **WGC（Windows Graphics Capture）** 窗口级捕获
- 跨平台降级路径：`QScreen::grabWindow()`

详见 [DESIGN.md](DESIGN.md)

---

### AudioCapturer / AudioPlayer — 语音

- 采样率 16000 Hz，单声道，16-bit PCM
- `AudioCapturer` emit `audioDataReady(QByteArray)`
- `AudioPlayer::playData(QByteArray)` 接收远端音频直接播放

详见 [AUDIO_DESIGN.md](AUDIO_DESIGN.md)

---

### AnnotationOverlay — 协同标注

- 透明绘图层覆盖在共享画面 QLabel 之上
- 本地绘制完成后 emit `strokeFinished(Stroke)`，供 Sender 转发
- 远端调用 `addStroke(Stroke)` 还原笔划；`clearAll()` 清空

详见 [ANNOTATION_DESIGN.md](ANNOTATION_DESIGN.md)

---

### Sender — 多路媒体发送

- 支持多路流注册（主视频、PiP、音频、笔划、文字）
- 四级优先级队列：Critical（音频）> High（标注）> Normal（主视频）> Low
- 编码器可替换（当前：JPEG 视频 + PCM 音频）
- 传输层可替换（当前：`DebugTransport` 调试模式，待接入真实 TCP/UDP）

详见 [SENDER_DESIGN.md](SENDER_DESIGN.md)

---

## 各模块协作信号连接示例

```cpp
// 屏幕帧 → Sender
connect(capturer, &ScreenCapturer::frameCaptured,
        sender,   &Sender::onMainScreenFrameCaptured);

// 音频 → Sender
connect(audioCapturer, &AudioCapturer::audioDataReady,
        sender,        &Sender::onAudioDataReady);

// 标注笔划 → Sender
connect(overlay, &AnnotationOverlay::strokeFinished,
        sender,  &Sender::onStrokePacketReady);

// 启动
capturer->start(30);
audioCapturer->start();
sender->start();
```

---

## 分支说明

| 分支 | 内容 |
|------|------|
| `main` | 主线集成分支 |
| `wyd` | Sender 多流调度模块 + 音频模块 + 标注模块开发分支 |

---

## License

本项目为课程/团队协作项目，暂未指定开源许可证。
