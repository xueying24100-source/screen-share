# V8.2 共享结束同步与高清传输修复

本版本在 v8.1 UI 同步版基础上修复两个问题：

## 1. 远端结束共享清屏

之前一端点击“结束共享”后，其他端的大窗口会保留最后一帧共享图像。原因是共享端只停止本地采集，没有向会议内广播“共享已结束”的控制消息。

现在新增：

- `Sender::sendShareStopped()`
- `MediaReceiver::mainVideoStoppedReceived()`
- `MainWindow::onRemoteMainStopped()`

流程：

```txt
共享端点击结束共享
        ↓
Sender 发送 DesktopMain + share_off 控制包
        ↓
Host 转发给所有客户端
        ↓
远端 MediaReceiver 收到 share_off
        ↓
MainWindow 清空大窗口，恢复“等待共享”
```

## 2. 主共享普通模式改为无损高清

之前普通模式仍会把主共享画面限制到约 `1920×1080`，并使用 JPEG 编码，文字和 UI 边缘容易发糊。

现在普通模式：

- 主共享不再限制最大分辨率；
- 主共享视频质量设置为 `100`；
- `VideoEncodeWorker` 在质量为 `100` 时改用 PNG 无损编码；
- `VideoDecodeWorker` 不再写死 JPEG，而是自动识别 PNG/JPEG。

说明：

```txt
普通模式：原始分辨率 + PNG 无损，画质优先
流畅模式：1280×720 + JPEG，流畅优先
摄像头：仍保持小窗口低分辨率，避免网络和 CPU 压力过大
```

如果演示时觉得卡，可以勾选“流畅模式”；如果要求文字清楚，不勾选流畅模式。
