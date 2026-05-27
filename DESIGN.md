# 屏幕采集模块技术设计文档

## 模块定位

本模块（`ScreenCapturer`）是整个屏幕共享系统的**数据源头**，负责周期性截取本机屏幕或指定窗口画面，并以 `QImage` 形式通过 Qt 信号发射给发送模块。

```
[屏幕/窗口画面]
       ↓
ScreenCapturer（本模块）
       ↓  emit frameCaptured(QImage)
       ↓  emit frameMetadataChanged(CaptureFrameMetadata)
Sender（网络发送，队友负责）
       ↓  TCP/UDP 传输
Receiver（接收端，队友负责）
       ↓
[显示画面]
```

---

## 公开接口

### 启动 / 停止

| 方法 | 说明 |
|------|------|
| `start(int fps = 30)` | 采集主屏，默认 30fps |
| `startScreen(int screenIndex, int fps = 30)` | 采集指定序号的屏幕 |
| `startWindow(quintptr windowId, int fps = 30)` | 采集指定窗口句柄 |
| `stop()` | 停止采集，释放 WGC/DXGI 资源 |
| `isRunning()` | 查询运行状态 |
| `setOutputSize(QSize)` | 设置输出分辨率（默认 1280×720）|

### 信号

| 信号 | 说明 |
|------|------|
| `frameCaptured(const QImage&)` | 每帧截图完成后 emit，格式 `Format_RGB32` |
| `captureError(const QString&)` | 采集出错时 emit（如窗口已关闭）|
| `frameMetadataChanged(const CaptureFrameMetadata&)` | 帧元数据变化时 emit（分辨率、后端名等）|

### CaptureFrameMetadata

| 字段 | 类型 | 说明 |
|------|------|------|
| `sourceSize` | `QSize` | 采集源的原始分辨率 |
| `sourceGeometry` | `QRect` | 采集区域在屏幕坐标系中的位置 |
| `windowHandle` | `quintptr` | 窗口句柄（屏幕采集时为 0）|
| `backendName` | `QString` | 实际使用的采集后端名称 |
| `frameIndex` | `qint64` | 当前会话已采集帧数（从 1 累加）|

---

## 采集模式

### 屏幕模式（PrimaryScreen / IndexedScreen）

```
captureFrame()
    ↓
captureWithDXGI()    [Windows 优先，GPU直出，可截视频/游戏]
    ↓ 失败/超时
captureWithGrabWindow()   [Qt跨平台降级]
```

- **DXGI Desktop Duplication**：`captureWithDXGI()`
  - D3D11 设备 + `IDXGIOutputDuplication` 只初始化一次，复用于后续帧
  - `m_stagingTexture` 缓存 CPU 可读 Staging Texture，避免每帧重新创建
  - `DXGI_ERROR_ACCESS_LOST` 时自动重置并下次重建
  - 超时（`DXGI_ERROR_WAIT_TIMEOUT`）返回 false，本 tick 跳过，不降级
  - 首次真正失败才设 `m_useDXGI = false`，永久降级到 GrabWindow
  - 输出：缩放到 `m_outputSize`（默认 1280×720）

- **GrabWindow**：`captureWithGrabWindow()`
  - 使用 `QScreen::grabWindow(0)` 截取整屏
  - 支持多屏按 `m_screenIndex` 选择
  - 输出：同样缩放到 `m_outputSize`

### 窗口模式（Window）

窗口采集采用三级降级策略，依次尝试：

```
captureFrame()  [窗口模式]
    ↓  先检测 HWND 是否有效（IsWindow）
    ↓
① WGC（Windows Graphics Capture，Win10 1803+）
    ↓ 失败 / 连续黑帧 ≥ 4 次
② GDI / captureWithGdiWindow()
      ├─ PrintWindow(PW_RENDERFULLCONTENT)
      ├─ BitBlt
      └─ ScreenCrop（QScreen::grabWindow 截区域）
    ↓ 全部为空
emit captureError()
```

**WGC（`WgcWindowCaptureBackend`）**
- 仅 Windows 10 1803+（`isSupported()` 检查）
- `start(HWND)` 初始化 WinRT CaptureItem + D3D11 帧池 + Session
- `tryGetFrame()` 拉取最新帧（无新帧返回 null QImage，跳过本 tick）
- 支持 `lastFrameSize()` 获取原始帧尺寸写入 metadata
- 窗口模式不做缩放，**保留原始分辨率**输出

**黑帧检测（`pixmapLooksMostlyBlack`）**
- 均匀采样最多 80×80 像素点
- 亮度（RGB 最大通道）< 30 视为黑像素
- 黑像素占比 > 92% 则判定为黑帧
- WGC 连续 `m_blackFrameThreshold`（默认 4）帧黑帧时自动切换到 GDI

**GDI（`captureWithGdiWindow`）**
- `PrintWindow(PW_RENDERFULLCONTENT)` → `BitBlt` → `ScreenCrop` 三级降级
- 输出：**保留原始窗口分辨率**，不强制缩放

---

## 采集后端一览

| 后端名（backendName）| 适用场景 | 是否缩放 |
|---------------------|---------|---------|
| `DXGI` | 全屏（Windows，GPU直出）| ✅ 缩放至 outputSize |
| `GrabWindow` | 全屏（跨平台降级）| ✅ 缩放至 outputSize |
| `WGC` | 窗口（Win10 1803+）| ❌ 原始分辨率 |
| `PrintWindow` | 窗口（GDI 主路径）| ❌ 原始分辨率 |
| `BitBlt` | 窗口（GDI 次路径）| ❌ 原始分辨率 |
| `ScreenCrop` | 窗口（截屏裁剪兜底）| ❌ 原始分辨率 |

---

## 与队友的集成说明

Sender 队友连接主屏信号：
```cpp
connect(capturer, &ScreenCapturer::frameCaptured,
        sender,   &Sender::onMainScreenFrameCaptured);
```

监听元数据变化（可选，用于 UI 显示后端信息）：
```cpp
connect(capturer, &ScreenCapturer::frameMetadataChanged,
        this, [](const CaptureFrameMetadata& meta) {
    qDebug() << "Backend:" << meta.backendName
             << "Size:" << meta.sourceSize
             << "Frame#" << meta.frameIndex;
});
```

在 `Sender::onMainScreenFrameCaptured` 中将 QImage 压缩为 JPEG bytes 后发送：
```cpp
QByteArray bytes;
QBuffer buf(&bytes);
buf.open(QIODevice::WriteOnly);
frame.save(&buf, "JPEG", 75);
// 然后通过 socket 发送 bytes
```

---

## 屏幕/窗口枚举（SourceEnumerator）

`SourceEnumerator` 提供静态方法，用于在 UI 中列出可选采集源：

```cpp
// 枚举所有屏幕
QList<ScreenInfo> screens = SourceEnumerator::enumerateScreens();
// ScreenInfo: { index, name, resolution, geometry }

// 枚举所有窗口
QList<WindowInfo> windows = SourceEnumerator::enumerateWindows();
// WindowInfo: { handle, title, minimized }
```

然后将所选句柄传入 `startWindow(handle)` 或将序号传入 `startScreen(index)`。

---

## 状态机

```
Idle ──start()──► Starting ──采集成功──► Running
                                ↓
                          Recovering（黑帧/WGC切换）
                                ↓ 恢复后继续
                            Running
Running ──stop()──► Stopped
Running ──错误──►  Error
```

| 状态 | 含义 |
|------|------|
| `Idle` | 初始状态，未启动 |
| `Starting` | 正在初始化后端（WGC start）|
| `Running` | 正常采集中 |
| `Recovering` | 检测到黑帧，正在切换后端 |
| `Error` | 不可恢复错误 |
| `Stopped` | 已调用 stop() |

---

## 关键成员变量

| 成员 | 说明 |
|------|------|
| `m_useDXGI` | 是否继续尝试 DXGI（失败后永久置 false）|
| `m_wgcFailed` | WGC 是否已失败（置 true 后走 GDI）|
| `m_blackFrameCount` | 连续黑帧计数器 |
| `m_blackFrameThreshold` | 黑帧切换阈值（默认 4）|
| `m_outputSize` | 输出分辨率（默认 1280×720，屏幕模式有效）|
| `m_frameIndex` | 当前会话帧序号 |
| `m_d3dDevice / m_d3dContext` | D3D11 设备（DXGI，复用）|
| `m_duplication` | DXGI OutputDuplication 对象（复用）|
| `m_stagingTexture` | CPU 可读 Staging Texture（复用）|
| `m_wgcBackend` | WGC 后端实例（`WgcWindowCaptureBackend`）|
