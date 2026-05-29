# Windows 平台代码（wyd + jzy）

> 只有 Windows 编译时这里的代码才会被链接，macOS 完全无感。

## 谁放什么

| 文件 | 谁负责 | 说明 |
|---|---|---|
| `WinScreenCapturer.{h,cpp}` | wyd | 适配类，继承 `ss::ICapturer`，把现有 WGC/GDI 后端"翻译"成统一接口 |
| `WinSourceEnumerator.{h,cpp}` | jzy | 适配类，继承 `ss::ISourceEnumerator`，列屏幕和窗口 |
| `WinCaptureFactory.cpp` | wyd + jzy 协作 | 实现 `ss::createCapturer()` / `ss::createSourceEnumerator()` 的 Windows 版 |
| `internal/wgc/` | wyd | 原 WGC 后端代码原样搬过来 |
| `internal/gdi/` | wyd | 原 GDI 后端 |
| `internal/audio/` | wyd | 原音频混音 |

## 规则

- ❌ 不要 include `src/platform/macos/` 的任何东西
- ❌ 不要 include 客户端层（`src/client/`）的东西
- ✅ 只对外暴露 `WinScreenCapturer` / `WinSourceEnumerator`，其它都在 `internal/` 里
- ✅ 客户端通过 `ss::createCapturer()` 拿到 `ICapturer*`，不需要知道里面是 WGC 还是 GDI

## 工厂样板（wyd 写完发给 jzy 接）

```cpp
// WinCaptureFactory.cpp
#include "screen_share/CaptureFactory.h"
#include "WinScreenCapturer.h"
#include "WinSourceEnumerator.h"

namespace ss {
ICapturer* createCapturer(QObject* parent) {
    return new WinScreenCapturer(parent);
}
ISourceEnumerator* createSourceEnumerator() {
    return new WinSourceEnumerator();
}
}
```
