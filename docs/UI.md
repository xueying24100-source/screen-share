# UI 层：会议主窗口与界面组件

## 模块概述

UI 层由以下类组成：

| 类 | 文件 | 职责 |
|----|------|------|
| `MeetingMainWindow` | `meetingmainwindow.{h,cpp}` | 会议主窗口，协调所有模块生命周期 |
| `ShareSourcePicker` | `sharesourcepicker.{h,cpp}` | 共享源选择对话框（屏幕/窗口/选项）|
| `ShareToolbar` | `sharetoolbar.{h,cpp}` | 悬浮共享控制工具条 |
| `LocalPreviewWindow` | `localpreviewwindow.{h,cpp}` | 本地预览窗口（帧预览 + 音频电平）|
| `MainWindow` | `mainwindow.{h,cpp}` | 应用入口窗口（当前仅作调试用）|

---

## MeetingMainWindow

`MeetingMainWindow`（继承 `QMainWindow`）是整个应用的核心协调者，定义于 `meetingmainwindow.h`。

### 持有的模块实例

| 成员 | 类型 | 说明 |
|------|------|------|
| `m_capturer` | `ScreenCapturer*` | 屏幕/窗口采集 |
| `m_sender` | `Sender*` | 媒体发送调度 |
| `m_audioCapturer` | `AudioCapturer*` | 麦克风采集 |
| `m_systemAudioCapturer` | `SystemAudioCapturer*` | 系统声音采集 |
| `m_audioMixer` | `AudioMixer*` | 音频混音 |
| `m_localPlayback` | `AudioPlayer*` | 本地音频回放 |
| `m_annotationWindow` | `AnnotationWindow*` | 全屏批注叠加窗口 |
| `m_preview` | `LocalPreviewWindow*` | 本地预览窗口 |
| `m_toolbar` | `ShareToolbar*` | 悬浮工具条 |
| `m_audioThread` | `QThread*` | 音频采集线程 |
| `m_captureThread` | `QThread*` | 屏幕采集线程 |

### 主要私有方法

| 方法 | 说明 |
|------|------|
| `startSharing(const ShareSelection&)` | 根据用户选择启动采集和音频，连接所有模块信号 |
| `stopSharing()` | 停止采集、音频、发送，断开信号连接 |
| `updateToolbarPosition()` | 根据共享源窗口位置更新工具条坐标 |
| `applyAnnotationGeometry()` | 将批注窗口对齐到采集源在屏幕上的位置 |
| `ensurePreviewWindow()` | 懒初始化本地预览窗口 |
| `updatePreviewPosition()` | 更新预览窗口位置（随主窗口移动）|
| `handleCaptureError(const QString&)` | 弹窗提示采集错误 |
| `onFrameCaptured(const QImage&)` | 接收视频帧，转发给 `Sender` 和 `LocalPreviewWindow` |
| `refreshPreviewComposite()` | 合成视频帧与批注图层，刷新预览显示 |
| `composeFrameWithAnnotations(const QImage&)` | 合并视频帧和批注层为单张 `QImage` |
| `onMixedAudio(const QByteArray&)` | 接收混音 PCM，计算 dBFS 并更新预览窗音量指示 |

### 线程管理

`MeetingMainWindow` 在 `startSharing` 时将 `AudioCapturer` 移入 `m_audioThread`，将 `ScreenCapturer` 移入 `m_captureThread`，通过 Qt 跨线程信号槽传递数据，在 `stopSharing` 时退出线程并等待完成。

### 定时器

| 定时器 | 说明 |
|--------|------|
| `m_windowFollowTimer` | 定时更新工具条位置（跟随共享窗口移动）|
| `m_previewRefreshTimer` | 定时刷新预览窗口（含批注合成）|

---

## ShareSourcePicker

`ShareSourcePicker`（继承 `QDialog`）提供共享源选择对话框。

### ShareSelection 结构

定义于 `sharesourcepicker.h`，描述用户的共享配置：

| 字段 | 类型 | 说明 |
|------|------|------|
| `kind` | `ShareSelection::Kind` | 共享类型：`Screen` 或 `Window` |
| `screenIndex` | `int` | 选择的屏幕序号（`kind == Screen` 时有效）|
| `hwnd` | `quintptr` | 选择的窗口句柄（`kind == Window` 时有效）|
| `includeSystemAudio` | `bool` | 是否采集系统声音（默认 true）|
| `includeCursor` | `bool` | 是否在画面中包含鼠标光标（默认 true）|
| `showBorder` | `bool` | 是否显示 WGC 黄色边框提示（默认 true）|
| `fps` | `int` | 目标帧率（默认 30）|

### 对话框结构

- 使用 `QTabWidget` 分屏幕/窗口两个标签页
- 屏幕列表（`m_screenGrid`）和窗口列表（`m_windowGrid`）通过 `SourceEnumerator` 枚举填充
- 底部选项区：系统声音、光标、边框复选框，以及帧率下拉框（`m_fpsCombo`）
- 「开始共享」按钮（`m_startShareButton`）：选中项后才可用（`updateConfirmEnabled`）

### 公开接口

```cpp
ShareSelection selection() const;   // 获取用户最终选择
```

---

## ShareToolbar

`ShareToolbar`（继承 `QWidget`）是一个可拖动的悬浮工具条，显示在共享画面上方或旁边。

### 状态控制槽

| 方法 | 说明 |
|------|------|
| `setPaused(bool)` | 同步暂停按钮文本/状态 |
| `setAnnotationEnabled(bool)` | 同步批注按钮状态 |
| `setMicMuted(bool)` | 同步麦克风静音状态 |
| `setSystemAudioEnabled(bool)` | 同步系统声音启用状态 |
| `setLocalPlaybackEnabled(bool)` | 同步本地回放复选框状态 |

### 信号

| 信号 | 说明 |
|------|------|
| `pauseToggled(bool)` | 用户点击暂停/继续 |
| `annotationToggled(bool)` | 用户开关批注模式 |
| `micMuteToggled(bool)` | 用户切换麦克风静音 |
| `systemAudioToggled(bool)` | 用户切换系统声音 |
| `localPlaybackToggled(bool)` | 用户切换本地回放 |
| `backRequested()` | 用户点击返回 |
| `stopRequested()` | 用户点击停止共享 |

### 拖动支持

`ShareToolbar` 重写了 `mousePressEvent` / `mouseMoveEvent`，支持用户拖动工具条到屏幕任意位置。`enterEvent` / `leaveEvent` 用于鼠标悬停时的视觉反馈。

---

## LocalPreviewWindow

`LocalPreviewWindow`（继承 `QWidget`）展示本地采集预览和音频电平，定义于 `localpreviewwindow.h`。

### 视频预览

| 方法 | 说明 |
|------|------|
| `updateFrame(const QImage&)` | 更新预览帧（内部缩放到 `m_displaySize`）|
| `updateMetadata(const CaptureFrameMetadata&)` | 更新后端名称、分辨率、帧序号 |
| `showError(const QString&)` | 在预览区显示错误信息 |

### 音频电平可视化

| 槽 | 说明 |
|----|------|
| `updateMicLevel(double dbfs)` | 更新麦克风音量指示（dBFS，带平滑）|
| `updateSystemLevel(double dbfs)` | 更新系统声音音量指示 |
| `setDeviceLabels(const QString& micName, const QString& outName)` | 显示当前音频设备名称 |

预览窗包含以下 `QLabel` / `QWidget`：

| 成员 | 说明 |
|------|------|
| `m_previewLabel` | 视频帧预览 |
| `m_statusLabel` | 后端名 / 分辨率 / 帧率 / 帧序号 |
| `m_errorLabel` | 采集错误信息 |
| `m_micDbLabel` / `m_systemDbLabel` | 当前 dBFS 数值文本 |
| `m_micDeviceLabel` / `m_systemDeviceLabel` | 设备名称文本 |
| `m_micVolumeBar` / `m_systemVolumeBar` | 音量条（自定义 `QWidget`）|

帧率通过 `m_frameTimesMs`（`QQueue<qint64>`）计算近 N 帧的平均帧间隔，实时显示 fps。

---

## MainWindow

`MainWindow`（`mainwindow.{h,cpp}`）目前仅作为开发调试入口：

```cpp
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    auto* w = new AnnotationWindow();
    w->show();
}
```

正式运行时程序入口（`main.cpp`）直接实例化 `MeetingMainWindow`，`MainWindow` 仅保留供单独调试批注窗口。

---

## 程序入口（main.cpp）

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

Windows 下在 `main` 中初始化 WinRT 公寓（`single_threaded`），供 WGC 后端使用。
