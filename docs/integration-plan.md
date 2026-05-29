# 联调计划（统一接口版）

> 所有人开工前先同步 `dev` 分支：`git fetch origin && git merge origin/dev`
> 适配 PR 一律提到 `dev` 分支，不要提到 `main`。

---

## 一、当前进度对照

| 成员 | 模块 | 个人分支 | 当前产出 |
|---|---|---|---|
| wyd（韦燕丹，组长） | Win 屏幕采集 | `wyd` | `src/{app,common,media,network,platform,ui}`；`src/platform/windows/wgc` 已有 WGC 后端 + GDI 回退 + 音频混音 + 本地预览 + 标注 |
| jzy（蒋宗原） | Win 窗口枚举 | `jzy` | `src/{app,ui}`，结构搭好 |
| xyz（邢雨茁） | mac 屏幕/窗口采集 | `xyz` | `include/screen_share/ScreenCaptureManager.h`、`AnnotationTypes.h`；`src/macos/ScreenCaptureManager.mm`（ScreenCaptureKit） |
| yzz（俞哲钊） | mac 窗口枚举 | `yzz` | `mac-window-capture-yzz/`：`SourceEnumerator`、`ScreenCapturer`、`ShareSourcePicker` |
| hjj（黄俊杰） | mac 客户端框架 | `hjj` | `pages/{LoginPage,RoomPage}`、`network/{RoomServer,RoomClient}`（TCP+JSON 信令）、`widgets/ScreenView`（预留 `updateFrame(QImage)`） |
| zpn（赵芃年） | Win 客户端框架 | `zpn` | `client-windows-zpn/` |

---

## 二、整体目标

让 6 个人现有代码**通过同一套接口契约**互相对接，做到：

- 采集模块 / 枚举模块 / 客户端 / 网络层 谁先做完都不阻塞别人。
- 客户端代码 **零 `#ifdef`**，Windows 和 macOS 跑同一份。
- 后续加新平台、改内部实现，**不影响其他人**。

接口契约统一定义在：`include/screen_share/`

- `Types.h` — `SourceInfo` / `CaptureConfig` / `VideoFrame` / `AudioFrame`
- `ICapturer.h` — 采集器接口（启停 + 信号）
- `ISourceEnumerator.h` — 枚举器接口（列屏幕 / 列窗口）
- `CaptureFactory.h` — 工厂函数，客户端只 include 这一个
- `IMediaChannel.h` — 网络传输接口（第二阶段用）

---

## 三、目录约定（必读！）

`dev` 分支的目录结构已经定好，**每个人只能往自己负责的目录里加文件**，不要在别处新建文件，也不要把"个人小项目目录"（如 `mac-window-capture-yzz/`、`client-windows-zpn/`）整个搬进 `dev`。

```
screen-share/
├── include/screen_share/      ← 公共接口（已完成，不要动）
├── src/
│   ├── platform/
│   │   ├── windows/           ← 🪟 wyd + jzy 在这里
│   │   │   ├── WinScreenCapturer.{h,cpp}        ← wyd
│   │   │   ├── WinSourceEnumerator.{h,cpp}      ← jzy
│   │   │   ├── WinCaptureFactory.cpp            ← wyd + jzy 协作
│   │   │   └── internal/                        ← wyd 把原 WGC/GDI/音频塞这里
│   │   │
│   │   └── macos/             ← 🍎 xyz + yzz 在这里
│   │       ├── MacScreenCapturer.{h,mm}         ← xyz
│   │       ├── MacSourceEnumerator.{h,mm}       ← yzz
│   │       ├── MacCaptureFactory.mm             ← xyz + yzz 协作
│   │       └── internal/                        ← 原 ScreenCaptureKit / 枚举实现
│   │
│   ├── client/                ← 💻 hjj + zpn 客户端框架（跨平台共用）
│   │   ├── pages/             ← LoginPage / RoomPage 等
│   │   ├── widgets/           ← ScreenView / MemberList / ToolButton 等
│   │   ├── network/           ← RoomServer / RoomClient
│   │   └── main.cpp           ← 程序入口（两端共用）
│   │
│   └── common/                ← 公共工具（日志、JPEG 编解码等，任何人可加）
│
├── docs/                      ← 文档
├── CMakeLists.txt             ← 顶层 CMake，按平台条件链接（已建好，不要新建）
└── README.md
```

### 谁能动哪 — 一张表说清

| 谁 | 允许新增/修改的目录 | 严禁碰 |
|---|---|---|
| **wyd** | `src/platform/windows/`（含 `internal/wgc`、`internal/gdi`、`internal/audio`） | `macos/`、`client/` |
| **jzy** | `src/platform/windows/`（`WinSourceEnumerator.*`、合写 `WinCaptureFactory.cpp`） | `macos/`、`client/` |
| **xyz** | `src/platform/macos/`（`MacScreenCapturer.*` + `internal/`） | `windows/`、`client/` |
| **yzz** | `src/platform/macos/`（`MacSourceEnumerator.*` + `internal/`） | `windows/`、`client/` |
| **hjj** | `src/client/`（业务逻辑、信令、UI），`src/common/` | `src/platform/` |
| **zpn** | `src/client/`（如有 Win 特有 UI 调整也放这里） | `src/platform/` |

### 几条硬性规则

- ✅ `src/client/` 是跨平台共用代码，**禁止写 `#ifdef Q_OS_WIN` / `#ifdef Q_OS_MAC`**。平台差异在 `ss::createCapturer()` 里解决。
- ✅ 平台目录是"黑盒"：客户端只 include `include/screen_share/CaptureFactory.h`，**绝不直接 include `src/platform/...` 下的文件**。
- ✅ 把原代码搬过来即可（保留 git 历史可用 `git mv`），不需要重命名、不需要重构。
- ✅ 顶层只有一个 `CMakeLists.txt`，**不要每人一个 `.pro` / `CMakeLists.txt`**。CMake 已经做了 `WIN32` / `APPLE` 条件分支，自动把对应平台目录的文件加进编译。

### 各目录都附了一份 README，照着抄即可

- [`src/platform/windows/README.md`](../src/platform/windows/README.md) — 给 wyd + jzy
- [`src/platform/macos/README.md`](../src/platform/macos/README.md) — 给 xyz + yzz
- [`src/client/README.md`](../src/client/README.md) — 给 hjj + zpn
- [`src/common/README.md`](../src/common/README.md) — 公共工具

---

## 四、第一阶段：接口适配（核心原则——只包一层，不重写）

### 总验收标准

每个端能做到 **"本机自采自显"**：从工厂拿 `ICapturer`，启动采集，`frameReady` 信号触发后画面显示在自家客户端的 `ScreenView` 上。

### 各成员任务

#### 1. wyd（组长）— Windows 采集适配

| 项 | 内容 |
|---|---|
| 做什么 | 给现有 WGC/GDI 后端套一层壳，让它"看起来像"`ss::ICapturer` |
| 怎么干 | 新建 `src/platform/windows/WinScreenCapturer.{h,cpp}`，继承 `ss::ICapturer`，构造时持有原 WGC 实例；`start()` 翻译参数后调原 API；原后端发画面时 emit `frameReady` |
| 交付物 | `WinScreenCapturer.{h,cpp}` + `src/platform/windows/WinCaptureFactory.cpp` 实现 Win 版 `ss::createCapturer()` |
| 不做 | 不动 WGC/GDI 内部逻辑、不动音频混音、不动现有 UI |
| PR 到 | `dev` |

#### 2. jzy — Windows 窗口/屏幕枚举适配

| 项 | 内容 |
|---|---|
| 做什么 | 把窗口/屏幕枚举包成 `ss::ISourceEnumerator` |
| 怎么干 | 新建 `src/platform/windows/WinSourceEnumerator.{h,cpp}`，实现 `listDisplays()` / `listWindows()`，返回 `QVector<ss::SourceInfo>`。Windows HWND 直接塞进 `SourceInfo::id`（quint64 装得下） |
| 交付物 | `WinSourceEnumerator.{h,cpp}` + 在 `WinCaptureFactory.cpp` 里实现 Win 版 `ss::createSourceEnumerator()`（和 wyd 协作同一个文件） |
| PR 到 | `dev` |

#### 3. xyz — macOS 采集适配

| 项 | 内容 |
|---|---|
| 做什么 | 给现有 `ScreenCaptureManager` 套壳成 `ss::ICapturer` |
| 怎么干 | 新建 `src/platform/macos/MacScreenCapturer.{h,mm}`，构造时持有 `ScreenCaptureManager`；翻译参数（你的 `ScreenCaptureSourceInfo::SourceType` ↔ `ss::SourceInfo::Type`）；把内部 `frameCaptured(quint32,SourceType,QImage)` 信号转发为 `frameReady(ss::VideoFrame)` |
| 交付物 | `MacScreenCapturer.{h,mm}` + `src/platform/macos/MacCaptureFactory.mm` 实现 mac 版 `ss::createCapturer()` |
| PR 到 | `dev` |

#### 4. yzz — macOS 枚举适配

| 项 | 内容 |
|---|---|
| 做什么 | 把 `SourceEnumerator` 包成 `ss::ISourceEnumerator` |
| 怎么干 | 新建 `src/platform/macos/MacSourceEnumerator.{h,mm}`；`listDisplays()` 内部调你现有的 `enumerateScreens()`，把 `ScreenInfo` 转 `ss::SourceInfo`（`index` 当 id，`name`/`resolution` 一一对应）；`listWindows()` 同理 |
| 交付物 | `MacSourceEnumerator.{h,mm}` + 在 `MacCaptureFactory.mm` 实现 mac 版 `ss::createSourceEnumerator()`（和 xyz 协作同一个文件） |
| PR 到 | `dev` |

#### 5. hjj（mac 客户端）/ zpn（Win 客户端）— 接入统一接口

| 项 | 内容 |
|---|---|
| 做什么 | 客户端只通过 `ss::createCapturer()` / `ss::createSourceEnumerator()` 拿对象，不直接 include 平台代码 |
| 怎么干 | ① 在"开始分享"按钮里：`auto* e = ss::createSourceEnumerator();` 列源给用户选；② `m_capturer = ss::createCapturer(this);` 接 `frameReady` 信号；③ slot 里 `m_screenView->updateFrame(f.image);` |
| 交付物 | `RoomPage`（hjj）/ 对应客户端主页（zpn）能调起统一接口，本机自采自显 |
| 不做 | 不要写任何 `#ifdef Q_OS_WIN` / `#ifdef Q_OS_MAC` |
| PR 到 | `dev` |

### 第一阶段完成标志

- `dev` 分支能在 Windows 和 macOS 各自编译通过。
- 启动客户端 → 选源 → 看到本机画面在 `ScreenView` 实时刷新（≥ 10 fps）。

---

## 五、第二阶段：网络联调（局域网跨机）

### 任务

把分享端 `ICapturer::frameReady` 出来的帧，通过 `IMediaChannel` 送到观看端，观看端把帧灌到 `ScreenView`。

### 帧格式建议

```
[4B 长度] [1B 类型: 0=video, 1=audio] [payload]

video payload = [8B ptsMs][JPEG 二进制]
audio payload = [8B ptsMs][PCM 二进制]
```

### 各成员任务（方向先列出，第一阶段完成后再细化）

| 成员 | 任务 |
|---|---|
| hjj | 实现 `TcpMediaChannel : ss::IMediaChannel`，复用现有 `RoomServer / RoomClient`，新增视频/音频二进制帧通道 |
| wyd | 分享端：把 `VideoFrame` 经 JPEG 压缩后调 `IMediaChannel::sendVideo()` |
| zpn | 观看端 Win：订阅 `IMediaChannel::videoArrived`，解 JPEG 后调 `ScreenView::updateFrame` |
| hjj | 观看端 mac：同上 |
| xyz / yzz | 协助调试 mac 端编码/解码、性能 |
| jzy | 协助调试 Win 端，处理多源切换 |

### 第二阶段完成标志

局域网内：1 个分享端 + 至少 2 个观看端（含 Win + mac）能看到分享端的实时画面，延迟 < 500ms。

---

## 六、Git 协作流程

```bash
# 每次开工前
git fetch origin
git checkout 你的分支
git merge origin/dev          # 拉最新接口和目录骨架

# 开发完
git add . && git commit -m "feat: xxx 适配 ss::ICapturer"
git push origin 你的分支

# 在 GitHub 上提 PR 时，base 选 dev（不是 main）
```

- 所有适配 PR → `dev`
- `dev` 联调稳定后由 wyd 统一 PR 到 `main` 打 tag

---

## 七、有问题怎么办

- 接口疑问 / 改接口提案：在对应 Issue 评论里 @wyd
- 编译挂了：把报错贴群里，注明分支 + 平台
- 需要别人配合改东西：直接 @ 对方，不要默默等
