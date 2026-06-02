# v6 架构吸收说明

## 本次目标

本次修改不是用 `screen-share-dev-win` 整体覆盖当前版本，而是在 v5 已经稳定的 UI 和功能基础上，吸收它的目录分层、采集模块和文档结构。

## 保留 v5 的内容

- 主会议界面和共享选择页。
- 桌面/窗口/白板共享流程。
- 已打开窗口的真实缩略图。
- 最大化窗口后的自适应布局。
- 高清发送参数：普通模式约 1920×1080，流畅模式约 1280×720。
- 摄像头本地预览和远端小窗显示。
- 关闭摄像头时发送 `camera_off` 控制包并清空远端最后一帧。
- 多线程 JPEG 编码/解码。
- 本机双开 TCP 通信。

## 吸收的新结构

```txt
src/app          UI 层
src/annotation   批注层
src/audio        音频层
src/capture      采集层
src/media        媒体处理层
src/network      网络传输层
src/platform     平台相关实现
docs             文档
translations     翻译文件
```

## 新并入但暂未强制替换主链路的模块

- `src/capture/ScreenCapturer`
- `src/capture/SourceEnumerator`
- `src/platform/windows/wgc/WgcWindowCaptureBackend`

这些模块已经加入 CMake 编译，方便后续继续做窗口共享稳定性优化。当前 `MainWindow` 仍保留 v5 中已经验证过的采集逻辑，避免一次性替换导致新回归。

## 后续建议

下一步可以把 `MainWindow::captureScreen()` 中的屏幕/窗口采集逻辑逐步迁移到 `ScreenCapturer`：

1. 桌面共享接入 `ScreenCapturer::startScreen()`。
2. 窗口共享接入 `ScreenCapturer::startWindow()`。
3. 收到 `frameCaptured` 后再叠加鼠标和批注。
4. 使用 `releaseFrameSlot()` 和编码完成信号联动，形成完整背压机制。
5. 如果 WGC 对部分窗口表现更稳定，再把 WGC 作为窗口共享默认后端。

