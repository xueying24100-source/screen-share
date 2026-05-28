# Qt 前端接入示例

下面是一份最小接入示例，演示 Qt 客户端界面层如何使用 `ScreenCaptureManager`。

## 最小流程

```cpp
#include "screen_share/ScreenCaptureManager.h"

ScreenCaptureManager manager;
manager.setIncludeCurrentApplicationContent(false);
manager.setResolutionPreset(CaptureResolutionPreset::HD720);
manager.setCapturesAudio(true);

QVector<ScreenCaptureSourceInfo> displays = manager.enumerateDisplays();
QVector<ScreenCaptureSourceInfo> windows = manager.enumerateWindows();
```

## 在列表里展示来源

```cpp
for (const auto &info : windows) {
    auto *item = new QListWidgetItem(
        QString("%1 (%2 x %3)")
            .arg(info.name)
            .arg(info.size.width())
            .arg(info.size.height()));

    item->setData(Qt::UserRole, QVariant::fromValue(info));
    ui->sourceList->addItem(item);
}
```

## 用户选中某一项后保存来源标识

```cpp
auto info = currentItem->data(Qt::UserRole).value<ScreenCaptureSourceInfo>();

quint32 selectedId = info.id;
ScreenCaptureSourceInfo::SourceType selectedType = info.type;
```

## 启动本地预览或真正共享

```cpp
QObject::connect(&manager,
                 &ScreenCaptureManager::frameCaptured,
                 this,
                 [](quint32 sourceId,
                    ScreenCaptureSourceInfo::SourceType sourceType,
                    const QImage &frame) {
    Q_UNUSED(sourceId);
    Q_UNUSED(sourceType);
    // 这里可以更新本地预览，也可以继续交给编码/推流模块
});

QObject::connect(&manager,
                 &ScreenCaptureManager::audioDataCaptured,
                 this,
                 [](quint32 sourceId,
                    ScreenCaptureSourceInfo::SourceType sourceType,
                    const QByteArray &audioData,
                    int sampleRate,
                    int channelCount) {
    Q_UNUSED(sourceId);
    Q_UNUSED(sourceType);
    Q_UNUSED(audioData);
    Q_UNUSED(sampleRate);
    Q_UNUSED(channelCount);
    // 这里拿到的是系统音频 PCM 数据
});

manager.startCapture(selectedId, selectedType);
```

## 推荐的前端职责

- 屏幕页只调用 `enumerateDisplays()`
- 窗口页只调用 `enumerateWindows()`
- 列表展示 `name / size / scale`
- 持久化用户最终选择时，只保留 `id + type`
- 把 include-self / 分辨率 / 是否带音频 视为共享配置，不要混到来源结构体里
- 停止共享时记得调用 `stopCapture(selectedId, selectedType)` 或 `stopAllCaptures()`

## 标注接入建议

如果前端要做共享画面标注，直接使用：

- `include/screen_share/AnnotationTypes.h`
- `docs/ANNOTATION_PROTOCOL.md`

推荐做法：

- 共享画面继续来自 `frameCaptured(...)`
- 标注 overlay 单独放在画面上层
- 本地鼠标操作转成 `AnnotationCommand`
- 远端回放时也同样按 `AnnotationCommand` 重绘

## 怎么判断接口可用

前端拿到每个 `ScreenCaptureSourceInfo` 后，至少检查：

- `id > 0`
- `name` 非空
- `size.width() > 0`
- `size.height() > 0`
- `scale > 0`
- `type` 与当前页面模式一致

如果这些条件都成立，这个来源对象就可以正常交给前端页面和后续采集模块。
