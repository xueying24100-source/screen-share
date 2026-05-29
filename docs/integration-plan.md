# 联调计划（统一接口版）

## 1. 当前进度对照

| 成员 | 模块 | 当前产出（分支现状） |
|---|---|---|
| wyd（韦燕丹） | Win 屏幕采集 | `src/{app,common,media,network,platform,ui}`；`src/platform/windows/wgc` 已实现 WGC 后端 + GDI 回退 + 音频混音 + 本地预览 + 标注 |
| jzy（蒋宗原） | Win 窗口枚举 | `src/{app,ui}` |
| xyz（邢雨茁） | mac 采集 | `include/screen_share/ScreenCaptureManager.h`、`AnnotationTypes.h`；`src/macos/ScreenCaptureManager.mm`（ScreenCaptureKit） |
| yzz（俞哲钊） | mac 窗口枚举与显示 | `mac-window-capture-yzz/`：`SourceEnumerator`、`ScreenCapturer`、`ShareSourcePicker` |
| hjj（黄俊杰） | mac 客户端框架 | `pages/{LoginPage,RoomPage}`、`network/{RoomServer,RoomClient}`（TCP+JSON 房间信令，含房间/抢麦）、`widgets/ScreenView`（预留 `updateFrame(QImage)`） |
| zpn（赵芃年） | Win 客户端框架 | `client-windows-zpn/` |

## 2. 本周适配任务（只做包装，不重写）

- wyd：将 WGC/GDI 后端包装为 `WinScreenCapturer : public ss::ICapturer`，放在 `src/platform/windows/WinScreenCapturer.{h,cpp}`，并实现 Windows 版 `ss::createCapturer()`。
- jzy：将现有窗口枚举包装为 `WinSourceEnumerator : public ss::ISourceEnumerator`，并实现 Windows 版 `ss::createSourceEnumerator()`。
- xyz：将现有 `ScreenCaptureManager` 包装为 `MacScreenCapturer : public ss::ICapturer`。
- yzz：将现有 `SourceEnumerator` 包装为 `MacSourceEnumerator : public ss::ISourceEnumerator`。
- hjj / zpn：在 `ScreenView` 增加 slot，对接 `ss::ICapturer::frameReady`，用 `f.image` 渲染。

## 3. 联调里程碑

- 本周：各端先完成“自采自显”（本机采集 + 本机显示）。
- 下周：接入 `IMediaChannel`，完成跨机传输联调。

## 4. Git 流程建议

- 建议建立 `dev` 集成分支。
- 每位成员从个人分支发 PR 到 `dev`。
- `main` 仅保留稳定版本与公共契约。
