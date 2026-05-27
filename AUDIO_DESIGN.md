# 语音模块技术设计文档

## 模块定位

本模块由 `AudioCapturer`（采集）和 `AudioPlayer`（播放）组成，负责语音数据的输入输出闭环：
- `AudioCapturer` 从默认麦克风采集 PCM 数据，并通过 Qt 信号向发送模块提供数据。
- `AudioPlayer` 接收网络还原后的 PCM 数据并写入默认扬声器播放。

```
[麦克风]
    ↓
AudioCapturer（本模块）
    ↓  emit audioDataReady(QByteArray)
Sender（网络发送，队友负责）
    ↓  网络传输
Receiver（接收端，队友负责）
    ↓  playData(QByteArray)
AudioPlayer（本模块）
    ↓
[扬声器]
```

---

## 接口约定

| 项目 | 约定值 | 说明 |
|------|--------|------|
| 采样率 | 16000 Hz | 语音场景低带宽 |
| 声道数 | 1（单声道） | 降低传输体积 |
| 位深 | 16-bit | `QAudioFormat::Int16` |
| 数据格式 | PCM | 未压缩原始音频 |
| 帧大小 | 约 640 bytes / 20ms | `16000 * 2 * 0.02 = 640` |

---

## 与队友的集成说明

Sender 侧连接采集信号：
```cpp
connect(audioCapturer, &AudioCapturer::audioDataReady, sender, &Sender::onAudioDataReady);
```

Receiver 侧将网络收到的音频数据直接交给播放器：
```cpp
audioPlayer->playData(audioBytes);
```

## 实现进度（演进路线）

| 阶段 | 状态 | 内容 |
|---|---|---|
| A | ✅ 已完成 | 本地回放调试、麦/系统声混音（16k/1ch/Int16）、音量可视化 |
| B | 🚧 计划中 | libopus 编码 + UDP/RTP + 独立接收端进程 |
| C | 🚧 计划中 | JitterBuffer + Opus FEC/PLC |
| D | 🚧 计划中 | WebRTC APM (AEC/NS/AGC) + 设备选择 + SFU 多人 |

### 阶段 A 注意事项
- 本地回放仅用于开发调试，**外放扬声器会与麦克风形成回声啸叫**，必须使用耳机。
- 当前编码仍是 PCM 透传，sender 仍走 `DebugTransport`，无实际网络发送，对端不会听到声音。
- `AudioMixer` 使用线性插值重采样和简单加和削峰，**不做 3A 处理**，目的仅为打通本地反馈闭环。
