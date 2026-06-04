# macOS 来源枚举接口说明

这个目录里的 `ScreenCaptureManager` 是给 Qt 客户端层使用的 macOS 来源枚举模块。

目录结构：

- `include/screen_share/AnnotationTypes.h`
- `include/screen_share/ScreenCaptureManager.h`
- `src/macos/ScreenCaptureManager.mm`
- `pages/RoomPage.cpp`
- `widgets/ScreenView.cpp`
- `docs/`

## 目标

- 枚举当前用户会话里的所有屏幕
- 枚举当前用户会话里可分享的顶层窗口
- 把结果以 Qt 友好的结构体和 `QVector` 返回给前端界面层

## 核心结构

```cpp
struct ScreenCaptureSourceInfo {
    enum class SourceType {
        Display = 0,
        Window = 1
    };

    quint32 id;
    SourceType type;
    QString name;
    QSize size;
    float scale;
};
```

前端同学真正需要保存的是：

- `id`
- `type`

`name / size / scale` 主要用于界面展示。

## 对外接口

```cpp
enum class CaptureResolutionPreset {
    Native = 0,
    Half,
    HD720,
    HD1080
};

void setIncludeCurrentApplicationContent(bool include);
void setResolutionPreset(CaptureResolutionPreset preset);
void setCapturesAudio(bool enabled);

QVector<ScreenCaptureSourceInfo> enumerateDisplays();
QVector<ScreenCaptureSourceInfo> enumerateWindows();

bool startCapture(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType);
void stopCapture(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType);
void stopAllCaptures();
```

可选信号：

```cpp
void displaysEnumerated(const QVector<ScreenCaptureSourceInfo> &displays);
void windowsEnumerated(const QVector<ScreenCaptureSourceInfo> &windows);
void frameCaptured(quint32 sourceId,
                   ScreenCaptureSourceInfo::SourceType sourceType,
                   const QImage &frame);
void audioDataCaptured(quint32 sourceId,
                       ScreenCaptureSourceInfo::SourceType sourceType,
                       const QByteArray &audioData,
                       int sampleRate,
                       int channelCount);
void captureError(quint32 sourceId,
                  ScreenCaptureSourceInfo::SourceType sourceType,
                  const QString &error);
```

## 枚举规则

### 屏幕

- 使用 `ScreenCaptureKit` 的 shareable content
- 返回当前用户会话中所有可分享 display
- `size` 是像素尺寸
- `scale` 由 `pixels / bounds` 计算得到

### 窗口

- 使用 `ScreenCaptureKit` 的 shareable content
- 只保留 `layer == 0` 的顶层窗口
- 过滤当前不可见或尺寸无效的窗口
- 默认过滤当前 `screen_capture` app 自己的窗口，避免误选自身造成套娃预览
- 如果调用 `setIncludeCurrentApplicationContent(true)`，则会把当前 app 自己的窗口也放回列表
- 过滤宽高无效的窗口

## 可选配置

- `setIncludeCurrentApplicationContent(bool)`：控制是否包含当前 `screen_capture` 自己的内容
- `setResolutionPreset(CaptureResolutionPreset)`：控制采集输出分辨率，不影响枚举结果
- `setCapturesAudio(bool)`：控制是否同时输出系统音频 PCM 数据

## 前端接入建议

1. Qt 界面启动时创建一个 `ScreenCaptureManager`
2. 用户切到“屏幕”页时调用 `enumerateDisplays()`
3. 用户切到“窗口”页时调用 `enumerateWindows()`
4. 用返回的 `QVector<ScreenCaptureSourceInfo>` 填充列表
5. 用户点击某一项后，记录这个来源的 `id + type`
6. 如果要做本地预览或真正共享，直接调用 `startCapture(id, type)`
7. 视频数据从 `frameCaptured(...)` 收，音频数据从 `audioDataCaptured(...)` 收
8. 结束共享时调用 `stopCapture(...)` 或 `stopAllCaptures()`

## 标注接口交付

如果前端需要在共享画面上做标注，不要直接复用 macOS 采集层内部控件；统一使用：

- [ANNOTATION_PROTOCOL.md](./ANNOTATION_PROTOCOL.md)
- `include/screen_share/AnnotationTypes.h`

这一层只定义标注命令结构，不和具体传输方式绑定。

更直观的接入示例见：

- [QT_CLIENT_USAGE_EXAMPLE.md](./QT_CLIENT_USAGE_EXAMPLE.md)

## 当前客户端接入

合并后的客户端已经在 `RoomPage` 和 `ScreenView` 中接入 macOS 采集模块：

- `RoomPage` 点击共享后弹出屏幕/窗口来源选择。
- 用户确认后调用 `startCapture(id, type)` 启动本机采集预览。
- `frameCaptured(...)` 回调中的 `QImage` 会传给 `ScreenView::updateFrame(...)`。
- 停止共享时调用 `stopAllCaptures()` 并清空 `ScreenView` 画面。

旧的 qmake demo 已从合并后的客户端分支移除，当前以 CMake 客户端工程为主。
