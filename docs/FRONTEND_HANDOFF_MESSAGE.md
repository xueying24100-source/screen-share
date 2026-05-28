# 前端交接说明

我这边已经把 macOS 端“可分享来源枚举”模块整理好了，前端可以直接接。

代码位置：

- 画笔/标注协议：`include/screen_share/AnnotationTypes.h`
- 头文件：`include/screen_share/ScreenCaptureManager.h`
- 实现：`src/macos/ScreenCaptureManager.mm`
- 枚举验证 Demo：`src/demo/main.cpp`
- 接口说明：`docs/MACOS_SOURCE_INTERFACE.md`
- Qt 接入示例：`docs/QT_CLIENT_USAGE_EXAMPLE.md`
- 标注协议说明：`docs/ANNOTATION_PROTOCOL.md`

你这边主要用两个接口：

```cpp
QVector<ScreenCaptureSourceInfo> enumerateDisplays();
QVector<ScreenCaptureSourceInfo> enumerateWindows();
```

返回结构：

```cpp
struct ScreenCaptureSourceInfo {
    quint32 id;
    SourceType type;   // Display / Window
    QString name;
    QSize size;
    float scale;
};
```

前端真正需要保存的是：

- `id`
- `type`

`name / size / scale` 用来做来源列表展示。

共享前可以按需设置这几个配置：

```cpp
manager.setIncludeCurrentApplicationContent(false);   // 是否包含 screen_capture 自己
manager.setResolutionPreset(CaptureResolutionPreset::HD720);
manager.setCapturesAudio(true);                       // 是否同时带系统音频
```

推荐接法：

1. 屏幕页调用 `enumerateDisplays()`
2. 窗口页调用 `enumerateWindows()`
3. 把返回的 `QVector<ScreenCaptureSourceInfo>` 填到来源列表
4. 用户选中某一项时，保存它的 `id + type`
5. 开始共享时调用 `startCapture(id, type)`
6. 画面从 `frameCaptured(...)` 收，音频从 `audioDataCaptured(...)` 收
7. 停止时调用 `stopCapture(...)` 或 `stopAllCaptures()`

额外说明：

- 默认会过滤掉当前 demo/app 自己的窗口，避免误选自身；如果业务上要允许共享自己，可以打开 `setIncludeCurrentApplicationContent(true)`
- 列表里每个来源都应该满足：`id > 0`、`name` 非空、`size` 有效、`scale > 0`
- 如果你那边先只做界面，也可以先只消费枚举结果；真正接共享时再接 `startCapture / frameCaptured / audioDataCaptured`
- 如果你要接“共享画面标注”，不要依赖平台侧 UI，统一看 `AnnotationTypes.h` 里的 `AnnotationCommand`

---
