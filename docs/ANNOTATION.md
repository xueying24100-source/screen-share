# 批注层

## 模块概述

批注功能由两个类共同实现：

| 类 | 文件 | 职责 |
|----|------|------|
| `AnnotationOverlay` | `src/ui/annotation/annotationoverlay.{h,cpp}` | 透明绘图层，处理鼠标输入、笔迹渲染、文字批注、撤销/重做 |
| `AnnotationWindow` | `src/ui/annotation/annotationwindow.{h,cpp}` | 承载 `AnnotationOverlay` 的全屏透明顶层窗口，处理窗口生命周期 |

---

## 数据结构

所有批注相关数据结构定义于 `src/ui/annotation/annotationoverlay.h`。

### Stroke（完整笔划）

| 字段 | 类型 | 说明 |
|------|------|------|
| `points` | `QList<QPointF>` | 笔划点序列（按绘制顺序，坐标为 overlay 坐标系）|
| `color` | `QColor` | 画笔颜色 |
| `width` | `float` | 画笔线宽（默认 3.0f）|
| `isEraser` | `bool` | 是否为橡皮模式 |

### StrokePacket（网络传输单元）

用于将笔划增量事件逐步发往对端：

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `StrokeEventType` | 事件类型（见下表）|
| `strokeId` | `quint32` | 笔划唯一 ID |
| `point` | `QPointF` | 当前点坐标 |
| `color` | `QColor` | 画笔颜色 |
| `width` | `float` | 画笔线宽 |
| `isEraser` | `bool` | 是否为橡皮 |

**StrokeEventType 枚举：**

| 值 | 含义 |
|----|------|
| `Begin` | 新笔划开始 |
| `Point` | 追加点 |
| `End` | 笔划结束 |
| `Undo` | 撤销上一笔 |
| `Clear` | 清空全部 |

### TextAnnotation（文字批注）

| 字段 | 类型 | 说明 |
|------|------|------|
| `position` | `QPointF` | 文字框左上角（overlay 坐标系）|
| `text` | `QString` | 文字内容 |
| `color` | `QColor` | 文字颜色（默认红色）|
| `fontSize` | `int` | 字号（point size，默认 16）|

### AnnotationTool 枚举

| 值 | 说明 |
|----|------|
| `Pen` | 画笔模式 |
| `Eraser` | 橡皮模式 |
| `Text` | 文字输入模式 |

---

## AnnotationOverlay 接口

### 公开槽（工具控制）

| 槽 | 说明 |
|----|------|
| `setPenColor(const QColor&)` | 设置画笔颜色 |
| `setPenWidth(float)` | 设置画笔线宽 |
| `setEraserMode(bool)` | 切换橡皮模式 |
| `setTextMode(bool)` | 切换文字输入模式 |
| `setFontSize(int)` | 设置文字字号 |
| `setToolbarExcludeRect(const QRect&)` | 排除工具条区域（防止鼠标事件透传到工具条下方的批注层）|
| `clearAll()` | 清空所有笔划和文字 |
| `undo()` / `redo()` | 撤销/重做本地操作 |
| `commitTextInput()` / `cancelTextInput()` | 确认/取消当前文字输入 |

### 公开槽（远端同步）

| 槽 | 说明 |
|----|------|
| `addStroke(const Stroke&)` | 追加一条完整笔划（用于远端回放）|
| `applyRemotePacket(const StrokePacket&)` | 应用远端增量事件（逐点同步）|

### 查询方法

| 方法 | 说明 |
|------|------|
| `bool canUndo() const` | 是否可撤销 |
| `bool canRedo() const` | 是否可重做 |
| `bool isEditingText() const` | 是否正在输入文字 |
| `AnnotationTool currentTool() const` | 当前激活工具 |
| `bool hasRenderableContent() const` | 是否有可渲染的批注内容 |
| `QList<Stroke> strokes() const` | 获取所有已完成笔划 |
| `QImage renderAnnotationsToImage(const QSize&) const` | 将当前批注渲染为指定尺寸的 QImage（用于合成预览）|

### 信号

| 信号 | 说明 |
|------|------|
| `strokePacketReady(const StrokePacket&)` | 每个笔划增量事件就绪（Begin/Point/End），供 `Sender` 实时发送 |
| `strokeFinished(const Stroke&)` | 一笔完整笔划结束，含全部点序列 |
| `textAnnotationCreated(const TextAnnotation&)` | 文字批注确认提交 |
| `undoRedoChanged()` | 撤销/重做可用状态变化 |
| `toolChanged(AnnotationTool)` | 当前工具切换 |
| `contentChanged()` | 批注内容变化（笔划/文字增删）|
| `closeRequested()` | 用户按 Esc 请求退出批注模式（`MeetingMainWindow` 据此隐藏 overlay）|

---

## 渲染实现

- **缓存 Pixmap（`m_cachedPixmap`）**：已完成的笔划在 `renderStrokeToCache` 中增量渲染到缓存，避免每帧重绘所有历史笔划。
- **当前笔划实时渲染**：`m_currentStroke` 在 `paintEvent` 中叠加绘制，保证实时跟手效果。
- **平滑路径**：`buildSmoothPath` 将点序列转换为二次贝塞尔曲线，使笔迹更圆滑。
- **文字批注**：通过嵌入 `QLineEdit`（`m_textEditor`）进行输入，确认后以 `QPainter` 渲染到 overlay。

---

## 撤销/重做机制

撤销/重做仅作用于**本地操作**（不影响已发往对端的增量包）：

- 每次完成一笔（`mouseReleaseEvent`）或提交文字（`commitTextInput`）后，将 `UndoAction` 压入 `m_undoHistory`，同时清空 `m_redoHistory`。
- `undo()` 弹出 `m_undoHistory` 顶部，将其移入 `m_redoHistory`，并重建 `m_strokes` / `m_textAnnotations`。
- `redo()` 逆向操作。
- 撤销/重做状态变化后 emit `undoRedoChanged()`，通知 UI 更新按钮状态。

---

## AnnotationWindow 接口

`AnnotationWindow` 是一个无边框全屏透明 `QWidget`，内部持有 `AnnotationOverlay` 和一个简单的工具条 widget：

| 方法 / 信号 | 说明 |
|------------|------|
| `void setTargetGeometry(const QRect&)` | 设置 overlay 覆盖的目标矩形（通常为采集源在屏幕上的位置）|
| `void requestExit()` | 外部请求退出批注模式（等效于 Esc 键）|
| `QImage renderAnnotationsToImage(const QSize&) const` | 转发给内部 `AnnotationOverlay` |
| `bool hasRenderableContent() const` | 转发给内部 `AnnotationOverlay` |
| `void closed()` | 窗口关闭时发出（仅发射一次，由 `m_closedEmitted` 保护）|
| `void strokePacketReady(const StrokePacket&)` | 转发自 `AnnotationOverlay::strokePacketReady` |
| `void textAnnotationCreated(const TextAnnotation&)` | 转发自 `AnnotationOverlay::textAnnotationCreated` |
| `void contentChanged()` | 转发自 `AnnotationOverlay::contentChanged` |

---

## MeetingMainWindow 集成示例

```cpp
// 启动批注模式
m_annotationWindow = new AnnotationWindow();
m_annotationWindow->setTargetGeometry(captureGeometry);
m_annotationWindow->show();

// 批注数据 → Sender
connect(m_annotationWindow, &AnnotationWindow::strokePacketReady,
        m_sender,           &Sender::onStrokePacketReady);
connect(m_annotationWindow, &AnnotationWindow::textAnnotationCreated,
        m_sender,           &Sender::onTextAnnotationCreated);

// 批注内容变化 → 刷新本地预览（合成批注图层）
connect(m_annotationWindow, &AnnotationWindow::contentChanged,
        this, &MeetingMainWindow::refreshPreviewComposite);

// 窗口关闭
connect(m_annotationWindow, &AnnotationWindow::closed,
        this, [this]{ /* 更新工具条状态 */ });
```

### 预览合成

`MeetingMainWindow::composeFrameWithAnnotations` 将最新视频帧与 `AnnotationWindow::renderAnnotationsToImage` 的输出合并为一张 `QImage`，送给 `LocalPreviewWindow` 展示。
