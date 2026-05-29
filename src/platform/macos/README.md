# macOS 平台代码（xyz + yzz）

> 只有 macOS 编译时这里的代码才会被链接，Windows 完全无感。

## 谁放什么

| 文件 | 谁负责 | 说明 |
|---|---|---|
| `MacScreenCapturer.{h,mm}` | xyz | 适配类，继承 `ss::ICapturer`，包装现有 `ScreenCaptureManager` |
| `MacSourceEnumerator.{h,mm}` | yzz | 适配类，继承 `ss::ISourceEnumerator`，包装现有 `SourceEnumerator` |
| `MacCaptureFactory.mm` | xyz + yzz 协作 | 实现 `ss::createCapturer()` / `ss::createSourceEnumerator()` 的 macOS 版 |
| `internal/ScreenCaptureManager.{h,mm}` | xyz | 原 ScreenCaptureKit 实现（从 xyz 分支 `src/macos/` 搬过来） |
| `internal/SourceEnumerator.{h,cpp}` | yzz | 原枚举实现（从 yzz 分支 `mac-window-capture-yzz/` 搬过来） |

## 规则

- ❌ 不要 include `src/platform/windows/` 的任何东西
- ❌ 不要 include 客户端层（`src/client/`）的东西
- ✅ 只对外暴露 `MacScreenCapturer` / `MacSourceEnumerator`
- ✅ 客户端通过 `ss::createCapturer()` 拿到 `ICapturer*`，不需要知道里面是 ScreenCaptureKit 还是别的

## 工厂样板

```objective-c++
// MacCaptureFactory.mm
#include "screen_share/CaptureFactory.h"
#import "MacScreenCapturer.h"
#import "MacSourceEnumerator.h"

namespace ss {
ICapturer* createCapturer(QObject* parent) {
    return new MacScreenCapturer(parent);
}
ISourceEnumerator* createSourceEnumerator() {
    return new MacSourceEnumerator();
}
}
```
