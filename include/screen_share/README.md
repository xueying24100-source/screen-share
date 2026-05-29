# screen_share 公共接口说明

## 约定

- 公共命名空间统一使用 `ss::`。
- 所有公共类型放在 `include/screen_share/` 下。
- 客户端层（hjj/zpn）只需包含 `screen_share/CaptureFactory.h`，不直接依赖平台实现。

## 平台实现规则

- Windows 实现放在 `src/platform/windows/`。
- macOS 实现放在 `src/platform/macos/`。
- `CaptureFactory.h` 只做声明，不做实现。
- 各平台分别在自己的 `.cpp` 中实现 `ss::createCapturer()` 与 `ss::createSourceEnumerator()`，编译时按平台条件二选一链接。

## 最小用法示例

```cpp
auto* enumerator = ss::createSourceEnumerator();
auto displays = enumerator->listDisplays();

auto* cap = ss::createCapturer(this);
connect(cap, &ss::ICapturer::frameReady,
        this, [](const ss::VideoFrame& f){ /* screenView->updateFrame(f.image); */ });
cap->start(displays.first(), ss::CaptureConfig{});
```
