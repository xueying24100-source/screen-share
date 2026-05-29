# 客户端代码（hjj + zpn）

> 跨平台共用。这里的代码 Windows 和 macOS 都会编译，所以**禁止使用任何 `#ifdef Q_OS_WIN` / `#ifdef Q_OS_MAC`**。

## 谁放什么

| 子目录 | 内容 | 主要负责人 |
|---|---|---|
| `pages/` | `LoginPage`、`RoomPage` 等页面 | hjj（hjj 分支已有，搬过来即可） |
| `widgets/` | `ScreenView`、`MemberList`、`ToolButton` 等控件 | hjj（已有） |
| `network/` | `RoomServer`、`RoomClient`（TCP+JSON 房间信令） | hjj（已有） |
| `main.cpp` | 程序入口 | hjj（mac）+ zpn（Win）合作一份 |

> zpn 的 Win 客户端如果有 Windows 特有 UI 调整，也放在 `pages/` 里通过 Qt 的运行时机制处理（不要用宏切代码）。

## 如何接入采集模块

客户端**只 include 一个文件**：

```cpp
#include "screen_share/CaptureFactory.h"
#include "screen_share/ICapturer.h"
#include "screen_share/ISourceEnumerator.h"

void RoomPage::onStartShareClicked() {
    // 1) 列源
    auto* enumerator = ss::createSourceEnumerator();
    QVector<ss::SourceInfo> displays = enumerator->listDisplays();
    ss::SourceInfo chosen = displays.first();  // 实际让用户选

    // 2) 创建采集器
    m_capturer = ss::createCapturer(this);

    // 3) 接信号
    connect(m_capturer, &ss::ICapturer::frameReady,
            this, [this](const ss::VideoFrame& f) {
                m_screenView->updateFrame(f.image);
            });

    // 4) 启动
    ss::CaptureConfig cfg;
    cfg.fps = 15;
    m_capturer->start(chosen, cfg);
}
```

## 规则

- ❌ 不要 include `src/platform/` 下任何文件
- ❌ 不要写 `#ifdef Q_OS_WIN` / `#ifdef Q_OS_MAC`
- ✅ 所有平台差异都在 `createCapturer()` 内部解决，客户端层零感知
