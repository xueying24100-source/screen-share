# Screen Share

**平台**：Windows 10/11（完整功能） | **Qt**：Qt6（Core / Gui / Widgets / Multimedia） | **构建状态**：✅ 按本文档可手动构建 | **License**：❌ 暂未指定

基于 **Qt6 + C++20** 的桌面屏幕共享应用，支持屏幕共享、窗口共享、实时音频传输、协同批注叠加以及本地预览监控。

---

## 主要功能

| 功能 | 说明 |
|------|------|
| 🖥️ 屏幕共享 | 全屏或指定屏幕采集，优先使用 `DXGI` GPU 直出，跨平台降级至 `GrabWindow` |
| 🪟 窗口共享 | 指定窗口采集，优先使用 `WGC`（Windows 10 1803+），自动降级至 `GDI`（`PrintWindow` / `BitBlt`） |
| 🎙️ 麦克风音频 | 16 kHz / 单声道 / 16-bit PCM 采集，支持静音控制 |
| 🔊 系统声音 | `WASAPI` loopback 采集系统播放声音，可与麦克风混音后发送 |
| 🎚️ 音频混音 | `AudioMixer` 双路混音，支持增益调节和人声 ducking 策略 |
| ✏️ 协同批注 | 透明绘图层叠加在共享画面上，支持笔迹、橡皮、文字、撤销 / 重做及远端同步 |
| 📺 本地预览 | `LocalPreviewWindow` 显示采集画面、后端信息、帧率和音量电平；主线程以 33 ms 定时器合并刷新 |
| 📡 多流发送 | `Sender` 汇聚视频 / 音频 / 批注流，优先级队列调度；运行在独立 sender thread，主线程通过 queued connection 投递 |

---

## 系统要求

| 项 | 要求 |
|------|------|
| **操作系统** | Windows 10/11（完整功能）；macOS / Linux 仅支持 `GrabWindow` 采集路径 |
| **Qt** | 6.x（Core / Gui / Widgets / Multimedia） |
| **CMake** | ≥ 3.16 |
| **编译器** | C++20（MSVC 2022 推荐；GCC 12+ / Clang 14+ 亦可） |
| **Windows SDK** | 10.0.19041.0+，含 C++/WinRT 头文件（`WGC` 后端必需） |

---

## 快速构建

```bash
git clone https://github.com/xueying24100-source/screen-share.git
cd screen-share
git checkout wyd

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt安装路径>/msvc2022_64"
cmake --build build --parallel
```

如需编译调试窗口（`src/ui/main/mainwindow.*`、`src/platform/windows/debug/wgctestwindow.*`），请额外启用 `-DSCREENSHARE_BUILD_DEBUG_WINDOWS=ON`。

---

## 文档索引

| 文档 | 说明 |
|------|------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 总体架构、模块关系图、数据流、线程模型与析构顺序 |
| [docs/CAPTURE.md](docs/CAPTURE.md) | 屏幕 / 窗口采集、`DXGI` / `WGC` / `GDI` 多后端策略、背压机制 |
| [docs/AUDIO.md](docs/AUDIO.md) | 麦克风采集、系统声音 loopback、混音器、本地回放 |
| [docs/NETWORK.md](docs/NETWORK.md) | `Sender` 协议封包、优先级队列、sender thread 调度 |
| [docs/UI.md](docs/UI.md) | 主控窗口、共享源选择、悬浮工具条、本地预览窗口 |
| [docs/ANNOTATION.md](docs/ANNOTATION.md) | 批注层数据结构、渲染、撤销 / 重做、远端同步 |
| [docs/BUILD.md](docs/BUILD.md) | 环境依赖、编译步骤、Qt Creator 集成、常见问题 |

---

## 目录结构

```text
screen-share/
├── CMakeLists.txt
├── README.md
├── docs/                                  # 技术文档
│   ├── ANNOTATION.md
│   ├── ARCHITECTURE.md
│   ├── AUDIO.md
│   ├── BUILD.md
│   ├── CAPTURE.md
│   ├── NETWORK.md
│   └── UI.md
├── src/
│   ├── app/
│   │   └── main.cpp                       # 程序入口，初始化 WinRT 公寓并启动 MeetingMainWindow
│   ├── ui/
│   │   ├── main/
│   │   │   ├── mainwindow.{h,cpp}         # 调试入口窗口（默认不编译，需 -DSCREENSHARE_BUILD_DEBUG_WINDOWS=ON）
│   │   │   └── meetingmainwindow.{h,cpp}  # 主控窗口，协调采集、发送、批注与预览
│   │   ├── picker/
│   │   │   └── sharesourcepicker.{h,cpp}  # 共享源选择对话框
│   │   ├── toolbar/
│   │   │   └── sharetoolbar.{h,cpp}       # 悬浮共享控制工具条
│   │   ├── preview/
│   │   │   └── localpreviewwindow.{h,cpp} # 本地预览窗口
│   │   └── annotation/
│   │       ├── annotationoverlay.{h,cpp}  # 透明绘图层（笔迹 / 橡皮 / 文字 / 撤销 / 重做）
│   │       └── annotationwindow.{h,cpp}   # 顶层透明批注窗口
│   ├── media/
│   │   ├── capture/
│   │   │   ├── screen/
│   │   │   │   ├── screencapturer.{h,cpp}   # 屏幕 / 窗口采集，多后端（DXGI / WGC / GDI）
│   │   │   │   └── sourceenumerator.{h,cpp} # 枚举屏幕和可见窗口
│   │   │   └── audio/
│   │   │       ├── audiocapturer.{h,cpp}       # 麦克风采集（Qt Multimedia）
│   │   │       └── systemaudiocapturer.{h,cpp} # 系统声音 loopback（WASAPI）
│   │   ├── mixer/
│   │   │   └── audiomixer.{h,cpp}          # 双路 PCM 混音、重采样、ducking
│   │   └── playback/
│   │       └── audioplayer.{h,cpp}         # PCM 播放（Qt Multimedia）
│   ├── network/
│   │   └── sender.{h,cpp}                 # 多路媒体流汇聚、优先级队列、协议封包
│   ├── platform/
│   │   └── windows/
│   │       ├── wgc/
│   │       │   └── wgcwindowcapturebackend.{h,cpp} # WGC 后端
│   │       └── debug/
│   │           └── wgctestwindow.{h,cpp}          # WGC 调试窗口（默认不编译，需 -DSCREENSHARE_BUILD_DEBUG_WINDOWS=ON）
│   └── common/                            # 预留通用工具（当前为空）
```

---

## License

本项目为课程 / 团队协作项目，暂未指定开源许可证。
