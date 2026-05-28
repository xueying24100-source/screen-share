# UI 层：主控窗口与界面组件

面向开发者、维护者与验收人员：本文说明主线程中的界面组件、共享控制流程，以及预览刷新与窗口跟随的实现方式。

## 模块概述

UI 层由以下模块组成：

| 模块 | 文件 | 职责 |
|------|------|------|
| `MeetingMainWindow` | `src/ui/main/meetingmainwindow.{h,cpp}` | 主控窗口，协调所有模块生命周期 |
| `ShareSourcePicker` | `src/ui/picker/sharesourcepicker.{h,cpp}` | 共享源选择对话框 |
| `ShareToolbar` | `src/ui/toolbar/sharetoolbar.{h,cpp}` | 悬浮共享控制工具条 |
| `LocalPreviewWindow` | `src/ui/preview/localpreviewwindow.{h,cpp}` | 本地预览窗口（帧预览 + 音频电平） |
| `MainWindow` | `src/ui/main/mainwindow.{h,cpp}` | 调试入口窗口（默认不编译，需 `-DSCREENSHARE_BUILD_DEBUG_WINDOWS=ON`） |

---

## MeetingMainWindow

`MeetingMainWindow`（继承 `QMainWindow`）是整个应用的主控窗口，定义于 `src/ui/main/meetingmainwindow.h`。

### 持有的模块实例

| 成员 | 类型 | 说明 |
|------|------|------|
| `m_capturer` | `ScreenCapturer*` | 屏幕 / 窗口采集 |
| `m_sender` | `Sender*` | 媒体发送调度 |
| `m_audioCapturer` | `AudioCapturer*` | 麦克风采集 |
| `m_systemAudioCapturer` | `SystemAudioCapturer*` | 系统声音采集 |
| `m_audioMixer` | `AudioMixer*` | 音频混音 |
| `m_localPlayback` | `AudioPlayer*` | 本地音频回放 |
| `m_annotationWindow` | `AnnotationWindow*` | 顶层批注窗口 |
| `m_preview` | `LocalPreviewWindow*` | 本地预览窗口 |
| `m_toolbar` | `ShareToolbar*` | 悬浮工具条 |
| `m_audioThread` | `QThread*` | audio thread |
| `m_captureThread` | `QThread*` | capture thread |
| `m_senderThread` | `QThread*` | sender thread |

### 主要私有方法

| 方法 | 说明 |
|------|------|
| `startSharing(const ShareSelection&)` | 根据用户选择启动采集、音频与发送流程 |
| `stopSharing()` | 停止采集、音频、发送并回收预览 / 批注窗口 |
| `updateToolbarPosition()` | 根据共享源位置更新工具条坐标 |
| `applyAnnotationGeometry()` | 将批注窗口对齐到采集源在屏幕中的位置 |
| `ensurePreviewWindow()` | 懒初始化本地预览窗口 |
| `updatePreviewPosition()` | 更新预览窗口位置 |
| `handleCaptureError(const QString&)` | 弹窗提示采集错误 |
| `onFrameCaptured(const QImage&)` | 接收视频帧，转发到 sender thread，并更新预览缓存 |
| `refreshPreviewComposite()` | 标记预览 dirty，并等待 33 ms 定时器统一刷新 |
| `composeFrameWithAnnotations(const QImage&)` | 合并视频帧和批注层 |
| `onMixedAudio(const QByteArray&)` | 记录混音日志并更新预览音量指示 |

### 线程管理

- `AudioCapturer`、`SystemAudioCapturer`、`AudioMixer`、`AudioPlayer` 被移入 **audio thread**。
- `ScreenCapturer` 被移入 **capture thread**。
- `Sender` 被移入 **sender thread**。
- 主线程与三个工作线程之间统一通过 queued connection 或 `QMetaObject::invokeMethod()` 传递数据，避免跨线程直接访问对象状态。

### 定时器

| 定时器 | 说明 |
|------|------|
| `m_windowFollowTimer` | 窗口共享时每 100 ms 更新批注层与工具条位置 |
| `m_previewRefreshTimer` | 每 33 ms 合并刷新一次本地预览，复用视频帧和批注变化通道 |

---

## 共享启动与停止流程

### 启动流程

```text
ShareSourcePicker 返回 ShareSelection
    → MeetingMainWindow::startSharing()
    → 在 capture thread 中配置 WGC 选项
    → 启动 ScreenCapturer / AudioCapturer / SystemAudioCapturer / AudioMixer
    → 显示 ShareToolbar 与 LocalPreviewWindow
    → 启动 33 ms 预览刷新定时器
```

### 停止流程

```text
MeetingMainWindow::stopSharing()
    → 停止预览与窗口跟随定时器
    → 停止本地回放 / 混音 / 麦克风 / 系统声音
    → 在 capture thread 中停止 ScreenCapturer
    → 关闭批注窗口、工具条和预览窗口
    → 清空缓存与状态
```

析构阶段还会进一步调用 `Sender::stop()`，并在各自线程中 `deleteLater()` 相关对象，再 `quit()/wait()` 三条工作线程，避免 queued 事件命中已销毁对象。

---

## ShareSourcePicker

`ShareSourcePicker`（继承 `QDialog`）提供共享源选择对话框。

### ShareSelection 结构

定义于 `src/ui/picker/sharesourcepicker.h`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `kind` | `ShareSelection::Kind` | 共享类型：`Screen` 或 `Window` |
| `screenIndex` | `int` | 选择的屏幕序号（`kind == Screen` 时有效） |
| `hwnd` | `quintptr` | 选择的窗口句柄（`kind == Window` 时有效） |
| `includeSystemAudio` | `bool` | 是否采集系统声音 |
| `includeCursor` | `bool` | 是否包含鼠标光标 |
| `showBorder` | `bool` | 是否显示 `WGC` 黄色边框提示 |
| `fps` | `int` | 目标帧率（默认 30） |

### 公开接口

```cpp
ShareSelection selection() const;
```

---

## ShareToolbar

`ShareToolbar`（继承 `QWidget`）是可拖动的悬浮工具条。

| 类别 | 项 | 说明 |
|------|------|------|
| 状态槽 | `setPaused(bool)` | 同步暂停按钮状态 |
| 状态槽 | `setAnnotationEnabled(bool)` | 同步批注按钮状态 |
| 状态槽 | `setMicMuted(bool)` | 同步麦克风静音状态 |
| 状态槽 | `setSystemAudioEnabled(bool)` | 同步系统声音状态 |
| 状态槽 | `setLocalPlaybackEnabled(bool)` | 同步本地回放复选框状态 |
| 用户信号 | `pauseToggled(bool)` | 用户点击暂停 / 继续 |
| 用户信号 | `annotationToggled(bool)` | 用户开关批注模式 |
| 用户信号 | `micMuteToggled(bool)` | 用户切换麦克风静音 |
| 用户信号 | `systemAudioToggled(bool)` | 用户切换系统声音 |
| 用户信号 | `localPlaybackToggled(bool)` | 用户切换本地回放 |
| 用户信号 | `backRequested()` | 用户点击返回 |
| 用户信号 | `stopRequested()` | 用户点击停止共享 |

---

## LocalPreviewWindow

`LocalPreviewWindow`（继承 `QWidget`）展示本地采集预览和音频电平。

| 类别 | 项 | 说明 |
|------|------|------|
| 视频预览 | `updateFrame(const QImage&)` | 更新预览帧 |
| 视频预览 | `updateMetadata(const CaptureFrameMetadata&)` | 更新后端名、分辨率、帧序号 |
| 视频预览 | `showError(const QString&)` | 显示采集错误 |
| 音频电平 | `updateMicLevel(double dbfs)` | 更新麦克风音量指示 |
| 音频电平 | `updateSystemLevel(double dbfs)` | 更新系统声音音量指示 |
| 音频电平 | `setDeviceLabels(const QString&, const QString&)` | 显示当前音频设备名称 |

预览窗口并不在每帧到达时立即重绘，而是由主线程统一维护 `m_lastRawFrame` 与 dirty flag，再通过 `m_previewRefreshTimer` 以 33 ms 周期推送到 `LocalPreviewWindow`，从而减少主线程在窗口共享场景下的刷新抖动。

---

## MainWindow 与程序入口

`MainWindow` 仅保留为调试入口窗口，默认不会被编入主可执行文件；需要显式启用 `-DSCREENSHARE_BUILD_DEBUG_WINDOWS=ON` 才会参与构建。

```cpp
int main(int argc, char* argv[])
{
#ifdef _WIN32
    winrt::init_apartment(winrt::apartment_type::single_threaded);
#endif
    QApplication app(argc, argv);
    MeetingMainWindow w;
    w.setWindowTitle(QStringLiteral("Screen Share"));
    w.resize(640, 360);
    w.show();
    const int code = app.exec();
#ifdef _WIN32
    winrt::uninit_apartment();
#endif
    return code;
}
```

Windows 下在 `main()` 中初始化 WinRT 公寓，供 `WGC` 后端使用；正式启动流程始终从 `MeetingMainWindow` 进入。

---

## 相关文档

| 文档 | 说明 |
|------|------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | 查看整体线程模型、sender thread 与析构顺序 |
| [CAPTURE.md](CAPTURE.md) | 查看采集链路、背压策略与窗口共享降级逻辑 |
| [ANNOTATION.md](ANNOTATION.md) | 查看批注窗口、批注图层与预览合成 |
| [TEST_CHECKLIST.md](TEST_CHECKLIST.md) | 查看屏幕共享、窗口共享和批注的验收步骤 |
