# 画笔标注模块技术设计文档

## 模块定位

本模块由 `AnnotationOverlay` 组成，是一个覆盖在共享画面之上的透明绘图层：
- 主共享人使用鼠标在叠加层上绘制标注线条。
- 接收端可通过网络还原笔划并调用接口显示，实现多人实时同步标注。

```
[共享画面 QLabel]
        ↑
AnnotationOverlay（透明绘图层）
```

---

## 数据流

```
[鼠标事件]
    ↓
AnnotationOverlay（本模块）
    ↓  emit strokeFinished(Stroke)
Sender（发送端，队友负责）
    ↓  网络传输
Receiver（接收端，队友负责）
    ↓  addStroke(Stroke)
AnnotationOverlay（远端显示）
```

---

## Stroke 数据结构

`Stroke` 表示一笔完整标注，包含：

| 字段 | 类型 | 说明 |
|------|------|------|
| `points` | `QList<QPoint>` | 当前笔划的点序列，按绘制顺序存储 |
| `color` | `QColor` | 画笔颜色 |
| `width` | `int` | 画笔线宽 |

本地绘制完成后，通过 `strokeFinished` 发出整笔数据；远端接收后使用 `addStroke()` 追加显示。

---

## 与队友集成方式

Sender 侧订阅笔划完成信号：
```cpp
connect(overlay, &AnnotationOverlay::strokeFinished,
        sender,  &Sender::onStrokeReady);
```

Receiver 侧收到网络数据后恢复为 `Stroke` 并添加到叠加层：
```cpp
overlay->addStroke(stroke);
```

清空操作可直接调用：
```cpp
overlay->clearAll();
```
