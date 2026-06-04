# 标注协议接口说明

这份协议是给 Qt 前端层直接使用的，目的不是定义“怎么画”，而是定义“标注命令长什么样”。

代码位置：

- `include/screen_share/AnnotationTypes.h`

## 设计原则

- 标注协议和 macOS 采集实现解耦
- 所有坐标都使用归一化坐标 `0.0 ~ 1.0`
- 前端本地绘制、远端同步、回放重建都使用同一份结构
- 不把 `QPainter`、鼠标事件、Cocoa 对象暴露给业务层

## 核心类型

```cpp
enum class AnnotationTool {
    Pen = 0,
    Rectangle,
    Arrow,
    Text,
    Clear
};

enum class AnnotationAction {
    Begin = 0,
    Update,
    Commit,
    ClearAll
};

struct AnnotationPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct AnnotationStyle {
    QColor color;
    float width = 3.0f;
};

struct AnnotationCommand {
    QString commandId;
    QString objectId;
    QString userId;
    AnnotationTool tool;
    AnnotationAction action;
    QVector<AnnotationPoint> points;
    QRectF normalizedRect;
    QString text;
    AnnotationStyle style;
    QSizeF sourceCanvasSize;
    qint64 timestampMs;
};
```

## 字段约定

- `commandId`：单条命令自己的唯一标识
- `objectId`：同一条标注对象的稳定标识，比如同一笔画的 begin/update/commit 应该复用同一个 `objectId`
- `userId`：发起标注的人
- `tool`：标注工具类型
- `action`：命令所处阶段
- `points`：画笔路径点；`Pen` 最常用
- `normalizedRect`：矩形类工具使用的归一化矩形
- `text`：文字标注预留字段
- `style`：颜色、线宽等视觉样式
- `sourceCanvasSize`：发送端生成这条命令时，对应共享画面的逻辑尺寸
- `timestampMs`：时间戳，方便排序和调试

## 坐标规范

所有点位和矩形都基于共享内容区域，而不是基于整个窗口。

例如主讲人的共享内容是 `1920 x 1080`：

- 左上角：`(0.0, 0.0)`
- 右下角：`(1.0, 1.0)`
- 中心点：`(0.5, 0.5)`

前端收到后，只需要乘以自己当前渲染出来的共享内容区域宽高，就能落到正确位置。

## 推荐生命周期

### 画笔

1. `Begin`
2. 多次 `Update`
3. `Commit`

### 矩形

1. `Begin`
2. 多次 `Update`
3. `Commit`

### 清空

- `tool = Clear`
- `action = ClearAll`

## 当前 demo 已覆盖的内容

合并后的客户端已经在 `widgets/ScreenView.cpp` 中使用本地 overlay 做标注验证：

- `Pen`
- `Rectangle`
- 归一化坐标
- 本地 overlay 绘制

所以这份协议不是“只写在文档里”，而是已经在 demo 中跑起来了。

## 交付给前端层的内容

1. 共享内容还是走 `ScreenCaptureManager`
2. 标注命令统一看 `AnnotationTypes.h`
3. 本地标注、远端同步、回放渲染都基于 `AnnotationCommand`
4. 不要依赖 macOS 采集层内部的 UI 或事件逻辑
