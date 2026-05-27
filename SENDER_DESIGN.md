# Sender 模块技术设计文档

## 模块定位

`Sender` 是整个屏幕共享系统的**媒体调度核心**，负责汇聚来自各采集模块的多路媒体流（视频帧、音频 PCM、标注笔划、文字标注），按优先级调度，经编码后通过可替换的网络传输层发送给接收端。

```
ScreenCapturer ──frameCaptured──► Sender ──► INetworkTransport ──► 接收端
AudioCapturer ──audioDataReady──►  │
AnnotationOverlay ─strokeFinished─►│
                   textAnnotation──►│
```

---

## 整体架构

```
                ┌──────────────────────────────────────┐
                │               Sender                 │
                │                                      │
  视频帧 ───────► onMainScreenFrameCaptured()           │
  PiP帧  ───────► onPipFrameCaptured()   IVideoEncoder │
  音频   ───────► onAudioDataReady()     IAudioEncoder │
  笔划   ───────► onStrokePacketReady()               │
  文字   ───────► onTextAnnotationCreated()            │
                │        ↓                             │
                │  优先级队列调度 (processSendLoop)     │
                │  Critical / High / Normal / Low       │
                │        ↓                             │
                │  buildPacket() → PacketHeader + payload│
                │        ↓                             │
                │  INetworkTransport::sendPacket()      │
                └──────────────────────────────────────┘
```

---

## 关键枚举与数据结构

### StreamType

| 值 | 说明 |
|----|------|
| `Video = 0` | 视频流（主屏 / PiP）|
| `Audio = 1` | 音频流 |
| `Annotation = 2` | 标注流（笔划 / 文字）|
| `Control = 3` | 控制流（预留）|

### SourceKind

| 值 | 说明 |
|----|------|
| `DesktopMain` | 主桌面截图 |
| `CameraPip` | 摄像头画中画 |
| `Microphone` | 麦克风音频 |
| `AnnotationStroke` | 标注笔划 |
| `AnnotationText` | 文字标注 |
| `SystemControl` | 系统控制（预留）|

### SendPriority

| 值 | 适用场景 |
|----|---------|
| `Critical = 0` | 音频（不可丢帧）|
| `High = 1` | 标注（实时性强）|
| `Normal = 2` | 主视频 |
| `Low = 3` | PiP / 低优先级帧 |

### PacketHeader（包头，固定 24 字节）

| 字段 | 类型 | 说明 |
|------|------|------|
| `version` | `quint8` | 协议版本（当前为 1）|
| `streamType` | `quint8` | 对应 `StreamType` 枚举 |
| `flags` | `quint16` | 扩展标志位（保留）|
| `streamId` | `quint32` | 流唯一标识 |
| `timestampUs` | `quint64` | 发送时刻（微秒） |
| `sequence` | `quint32` | 本流包序号（从 1 递增）|
| `payloadSize` | `quint32` | payload 长度（字节）|

---

## 编码器接口

### IVideoEncoder

```cpp
virtual QByteArray encode(const QImage& frame, int quality) = 0;
```

当前实现：`JpegVideoEncoder`，使用 Qt 内置 JPEG 编码，quality 由流配置决定（默认 75）。

### IAudioEncoder

```cpp
virtual QByteArray encode(const QByteArray& pcm) = 0;
```

当前实现：`PcmAudioEncoder`，直传原始 PCM（16000 Hz / 单声道 / 16-bit）。

> **升级路径**：后续可替换为 Opus 编码器，无需修改 Sender 本身，只需注入不同的 `IAudioEncoder` 实现。

---

## 网络传输接口

```cpp
class INetworkTransport {
public:
    virtual bool sendPacket(const QByteArray& packet, bool reliable) = 0;
};
```

- `reliable = true`：需要可靠送达（如标注、控制包），对应 TCP 语义。
- `reliable = false`：允许丢弃（如视频帧），对应 UDP 语义。
- 当前调试实现：`DebugTransport`，仅打印包信息，不做真实网络发送。

---

## 流注册与生命周期

```cpp
// 注册主视频流
StreamId vid = sender.registerVideoStream(SourceKind::DesktopMain,
                                          SendPriority::Normal, true, 30, 75);

// 注册音频流
StreamId aud = sender.registerAudioStream(SourceKind::Microphone,
                                          SendPriority::Critical, true);

// 注册标注笔划流
StreamId ann = sender.registerAnnotationStream(SourceKind::AnnotationStroke,
                                               SendPriority::High, true);

sender.start();   // 启动发送循环
// ...
sender.stop();    // 停止
```

每条流拥有独立的序号计数器（`nextSequence`）与限速器（`lastFrameTimestampUs`）。

---

## 优先级队列调度

`processSendLoop()` 由内部 `QTimer` 周期触发，每次按以下顺序尝试取包：

```
Critical → High → Normal → Low
```

视频队列在超出容量时会自动丢弃最旧的帧（`dropOldVideoPacketsForStream`），保证实时性不被积压影响。

---

## 与其他模块的集成

### 接入 ScreenCapturer

```cpp
connect(capturer, &ScreenCapturer::frameCaptured,
        sender,   &Sender::onMainScreenFrameCaptured);
```

### 接入 AudioCapturer

```cpp
connect(audioCapturer, &AudioCapturer::audioDataReady,
        sender,        &Sender::onAudioDataReady);
```

### 接入 AnnotationOverlay（笔划）

```cpp
connect(overlay, &AnnotationOverlay::strokeFinished,
        sender,  &Sender::onStrokePacketReady);
```

### 接入 AnnotationOverlay（文字）

```cpp
connect(overlay, &AnnotationOverlay::textAnnotationCreated,
        sender,  &Sender::onTextAnnotationCreated);
```

---

## 运行时控制

| 方法 | 说明 |
|------|------|
| `setTransport(INetworkTransport*)` | 切换网络传输实现 |
| `setTargetBitrate(quint32 bps)` | 设置目标码率（供自适应码率扩展使用）|
| `setMaxFps(StreamId, int)` | 动态调整某路视频最大帧率 |
| `setVideoQuality(StreamId, int)` | 动态调整某路视频编码质量 |
| `setStreamEnabled(StreamId, bool)` | 暂停/恢复某路流 |
| `unregisterStream(StreamId)` | 注销并释放流 |

---

## 扩展路线

| 阶段 | 内容 |
|------|------|
| 当前 | JPEG 视频 + PCM 音频 + DebugTransport |
| 近期 | 接入真实 TCP/UDP 传输（替换 `INetworkTransport`）|
| 中期 | 替换 `JpegVideoEncoder` 为 H.264/H.265 软编 |
| 远期 | 替换 `PcmAudioEncoder` 为 Opus，支持多路 PiP，自适应码率 |
