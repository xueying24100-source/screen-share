# 批注层

面向开发者、维护者与验收人员：本文说明批注数据结构、绘制逻辑、撤销 / 重做机制，以及批注内容如何进入 sender thread 与本地预览。

## 模块概述

批注功能由两个模块共同实现：

| 模块 | 文件 | 职责 |
|------|------|------|
| `AnnotationOverlay` | `src/ui/annotation/annotationoverlay.{h,cpp}` | 透明绘图层，处理鼠标输入、笔迹渲染、文字批注、撤销 / 重做 |
| `AnnotationWindow` | `src/ui/annotation/annotationwindow.{h,cpp}` | 承载 `AnnotationOverlay` 的顶层透明窗口，负责窗口生命周期 |

`tests/test_annotation_overlay.cpp` 覆盖撤销 / 重做、渲染结果和批注相关核心行为。

---

## 数据结构

所有批注数据结构定义于 `src/ui/annotation/annotationoverlay.h`。

### Stroke（完整笔划）

| 字段 | 类型 | 说明 |
|------|------|------|
| `points` | `QList<QPointF>` | 笔划点序列（按绘制顺序） |
| `color` | `QColor` | 画笔颜色 |
| `width` | `float` | 画笔线宽（默认 3.0f） |
| `isEraser` | `bool` | 是否为橡皮模式 |

### StrokePacket（网络传输单元）

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `StrokeEventType` | 事件类型 |
| `strokeId` | `quint32` | 笔划唯一 ID |
| `point` | `QPointF` | 当前点坐标 |
| `color` | `QColor` | 画笔颜色 |
| `width` | `float` | 画笔线宽 |
| `isEraser` | `bool` | 是否为橡皮 |

**`StrokeEventType` 枚举：**

| 值 | 含义 |
|------|------|
| `Begin` | 新笔划开始 |
| `Point` | 追加点 |
| `End` | 笔划结束 |
| `Undo` | 撤销上一笔 |
| `Clear` | 清空全部 |

### TextAnnotation（文字批注）

| 字段 | 类型 | 说明 |
|------|------|------|
| `position` | `QPointF` | 文字框左上角 |
| `text` | `QString` | 文字内容 |
| `color` | `QColor` | 文字颜色 |
| `fontSize` | `int` | 字号（默认 16） |

### AnnotationTool 枚举

| 值 | 说明 |
|------|------|
| `Pen` | 画笔模式 |
| `Eraser` | 橡皮模式 |
| `Text` | 文字输入模式 |

---

## AnnotationOverlay 接口

| 类别 | 项 | 说明 |
|------|------|------|
| 工具槽 | `setPenColor(const QColor&)` | 设置画笔颜色 |
| 工具槽 | `setPenWidth(float)` | 设置画笔线宽 |
| 工具槽 | `setEraserMode(bool)` | 切换橡皮模式 |
| 工具槽 | `setTextMode(bool)` | 切换文字输入模式 |
| 工具槽 | `setFontSize(int)` | 设置文字字号 |
| 工具槽 | `setToolbarExcludeRect(const QRect&)` | 排除工具条区域 |
| 工具槽 | `clearAll()` | 清空所有笔划和文字 |
| 工具槽 | `undo()` / `redo()` | 撤销 / 重做本地操作 |
| 工具槽 | `commitTextInput()` / `cancelTextInput()` | 确认 / 取消当前文字输入 |
| 远端同步 | `addStroke(const Stroke&)` | 追加完整笔划 |
| 远端同步 | `applyRemotePacket(const StrokePacket&)` | 应用远端增量事件 |
| 查询 | `canUndo() const` | 是否可撤销 |
| 查询 | `canRedo() const` | 是否可重做 |
| 查询 | `isEditingText() const` | 是否正在输入文字 |
| 查询 | `currentTool() const` | 当前激活工具 |
| 查询 | `hasRenderableContent() const` | 是否有可渲染内容 |
| 查询 | `strokes() const` | 获取所有已完成笔划 |
| 查询 | `renderAnnotationsToImage(const QSize&) const` | 将当前批注渲染为 `QImage` |
| 信号 | `strokePacketReady(const StrokePacket&)` | 笔划增量事件就绪 |
| 信号 | `strokeFinished(const Stroke&)` | 完整笔划结束 |
| 信号 | `textAnnotationCreated(const TextAnnotation&)` | 文字批注确认提交 |
| 信号 | `undoRedoChanged()` | 撤销 / 重做状态变化 |
| 信号 | `toolChanged(AnnotationTool)` | 当前工具切换 |
| 信号 | `contentChanged()` | 批注内容变化 |
| 信号 | `closeRequested()` | 用户请求退出批注模式 |

---

## 渲染实现

- **缓存 Pixmap（`m_cachedPixmap`）**：已完成笔划通过 `renderStrokeToCache()` 增量渲染到缓存，避免每帧重绘全部历史内容。
- **当前笔划实时渲染**：`m_currentStroke` 在 `paintEvent()` 中叠加绘制，保证跟手效果。
- **平滑路径**：`buildSmoothPath()` 将点序列转换为二次贝塞尔曲线，使笔迹更圆滑。
- **文字批注**：通过嵌入 `QLineEdit`（`m_textEditor`）进行输入，确认后再用 `QPainter` 渲染到 overlay。

---

## 撤销 / 重做机制

撤销 / 重做仅作用于**本地操作**：

- 完成一笔或提交文字后，将 `UndoAction` 压入 `m_undoHistory`，同时清空 `m_redoHistory`。
- `undo()` 将顶部动作移入 `m_redoHistory`，并重建 `m_strokes` / `m_textAnnotations`。
- `redo()` 执行逆向恢复。
- 状态变化后发射 `undoRedoChanged()`，通知工具条更新按钮状态。

---

## AnnotationWindow 接口

`AnnotationWindow` 是一个无边框透明 `QWidget`，内部持有 `AnnotationOverlay` 以及批注工具条部件：

| 方法 / 信号 | 说明 |
|------|------|
| `void setTargetGeometry(const QRect&)` | 设置 overlay 覆盖的目标矩形 |
| `void requestExit()` | 外部请求退出批注模式 |
| `QImage renderAnnotationsToImage(const QSize&) const` | 转发给内部 `AnnotationOverlay` |
| `bool hasRenderableContent() const` | 查询当前是否存在可渲染内容 |
| `void closed()` | 窗口关闭时发出 |
| `void strokePacketReady(const StrokePacket&)` | 转发自 `AnnotationOverlay::strokePacketReady` |
| `void textAnnotationCreated(const TextAnnotation&)` | 转发自 `AnnotationOverlay::textAnnotationCreated` |
| `void contentChanged()` | 转发自 `AnnotationOverlay::contentChanged` |

---

## MeetingMainWindow 集成示例

```cpp
connect(m_annotationWindow, &AnnotationWindow::strokePacketReady,
        m_sender, &Sender::onStrokePacketReady,
        Qt::QueuedConnection);

connect(m_annotationWindow, &AnnotationWindow::textAnnotationCreated,
        m_sender, &Sender::onTextAnnotationCreated,
        Qt::QueuedConnection);

connect(m_annotationWindow, &AnnotationWindow::contentChanged,
        this, [this]() {
            m_annotationLayerDirty = true;
            refreshPreviewComposite();
        });
```

主线程不会在每次 `contentChanged()` 时立即强制重绘预览；相反，它只更新 dirty flag，再复用 `MeetingMainWindow` 的 **33 ms 合并刷新定时器** 统一推送到 `LocalPreviewWindow`。

---

## 相关文档

| 文档 | 说明 |
|------|------|
| [UI.md](UI.md) | 查看批注窗口的创建、显示与几何跟随 |
| [NETWORK.md](NETWORK.md) | 查看笔划与文字如何进入 sender thread 并序列化 |
| [TEST_CHECKLIST.md](TEST_CHECKLIST.md) | 查看批注相关手动验收步骤 |
