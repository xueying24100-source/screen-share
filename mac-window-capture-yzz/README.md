# macOS 窗口采集 Demo

这是 `yzz` 分支的 macOS 窗口枚举与采集 demo

- `SourceEnumerator::enumerateScreens()`
- `SourceEnumerator::enumerateWindows()`
- `ScreenCapturer::startScreen()`
- `ScreenCapturer::startWindow()`
- `frameCaptured(const QImage&)`

当前版本使用 CoreGraphics：

- `CGWindowListCopyWindowInfo` 枚举窗口
- `CGWindowListCreateImage` 采集指定窗口
- `QScreen::grabWindow(0)` 采集屏幕

macOS 14 开始 `CGWindowListCreateImage` 会提示 deprecated。它适合课程 demo 和接口联调，后续正式版本可以在不改上层接口的前提下替换为 `ScreenCaptureKit`。

## 构建

```bash
/Users/zhezhaoyu/Qt/Tools/CMake/CMake.app/Contents/bin/cmake -B build -DCMAKE_PREFIX_PATH=/Users/zhezhaoyu/Qt/6.11.1/macos
/Users/zhezhaoyu/Qt/Tools/CMake/CMake.app/Contents/bin/cmake --build build
open build/mac_window_capture_yzz.app
```

如果采集不到画面，请在系统设置里允许屏幕录制权限：

`系统设置 -> 隐私与安全性 -> 屏幕与系统音频录制`

