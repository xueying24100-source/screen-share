# 构建说明

面向开发者、维护者与验收人员：本文汇总构建依赖、CMake 选项、测试入口与常见排障方法，适合在首次配置环境时对照使用。

## 环境要求

| 项 | 版本要求 | 说明 |
|------|------|------|
| **Qt** | 6.x | 需安装 `Core`、`Gui`、`Widgets`、`Multimedia` 模块；如需构建测试，还需 `Test` 模块 |
| **CMake** | ≥ 3.16 | 项目构建系统 |
| **编译器** | C++20 | Windows：MSVC 2022（推荐）；Linux / macOS：GCC 12+ / Clang 14+ |
| **操作系统** | Windows 10/11（完整功能） | macOS / Linux 仅支持 `GrabWindow` 采集路径，无 `DXGI` / `WGC` / `WASAPI` |
| **Windows SDK** | 10.0.19041.0+ | 需包含 WinRT 头文件（`winrt/base.h`、`windows.graphics.capture.h`）以支持 `WGC` 后端 |
| **C++/WinRT** | 随 Windows SDK 附带 | `WGC` 后端通过 C++/WinRT 投影访问 Windows.Graphics.Capture API |

> `WGC` 窗口共享需要 Windows 10 1803（RS4）及以上版本在**运行时**可用；构建时只需 SDK 版本满足要求即可。

---

## 依赖库（Windows 平台）

`CMakeLists.txt` 在 Windows 下自动链接以下系统库：

| 库 | 用途 |
|------|------|
| `d3d11` | `DXGI` Desktop Duplication（屏幕共享） |
| `dxgi` | `DXGI` 接口 |
| `user32` | 窗口操作（`IsWindow`、`PrintWindow` 等） |
| `windowsapp` | C++/WinRT 运行时（`WGC` 后端） |
| `ole32` | COM 初始化 |
| `mmdevapi` | `WASAPI` 设备枚举与 loopback 采集 |
| `propsys` | 音频设备属性查询 |
| `avrt` | 音频实时线程优先级 |

---

## CMake 关键配置

项目顶层 CMake 暴露了调试窗口和测试相关选项：

```cmake
cmake_minimum_required(VERSION 3.16)
project(ScreenShare_Capturer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(SCREENSHARE_BUILD_DEBUG_WINDOWS "Build standalone debug windows" OFF)
option(SCREENSHARE_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)
enable_testing()

if(SCREENSHARE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

常用选项说明：

| 选项 | 默认值 | 说明 |
|------|------|------|
| `SCREENSHARE_BUILD_DEBUG_WINDOWS` | `OFF` | 编译 `MainWindow` 和 `WgcTestWindow` 两个独立调试窗口 |
| `SCREENSHARE_BUILD_TESTS` | `ON` | 编译 `tests/` 下的 QtTest 可执行文件 |

---

## 构建步骤

### 1. 克隆仓库并切换分支

```bash
git clone https://github.com/xueying24100-source/screen-share.git
cd screen-share
git checkout wyd
```

### 2. 配置构建目录

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake -B build -DCMAKE_BUILD_TYPE=Debug

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSCREENSHARE_BUILD_DEBUG_WINDOWS=ON
```

如果 Qt6 未在系统路径中，需手动指定 Qt 安装目录：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
```

如需关闭测试目标，可追加 `-DSCREENSHARE_BUILD_TESTS=OFF`。

### 3. 编译

```bash
cmake --build build --parallel
```

### 4. 运行

```text
build/Release/ScreenShare_Capturer.exe   （Windows MSVC）
build/ScreenShare_Capturer               （Linux / macOS）
```

### 5. 运行测试

```text
ctest --test-dir build -C Debug --output-on-failure
ctest --test-dir build -C Debug -L Manual -V
```

第一条命令执行自动化测试；第二条命令单独触发带 `Manual` label 的 Windows 交互式冒烟测试。

---

## Qt Creator 集成

项目根目录包含 `.qtcreator/` 配置，可直接在 Qt Creator 中通过“打开项目”选择 `CMakeLists.txt` 导入。调整 `SCREENSHARE_BUILD_DEBUG_WINDOWS` 或 `SCREENSHARE_BUILD_TESTS` 后，建议重新执行一次 **Run CMake**。

---

## 常见问题

### WGC 相关编译错误

- **`winrt/base.h` 找不到**：确认已安装 Windows SDK 10.0.19041.0+，并在 CMake 中正确配置 MSVC 工具链。
- **`/await:strict` 不支持**：需使用 MSVC 2019 16.8 或更高版本（建议 Visual Studio 2022）。
- **`windowsapp.lib` 链接失败**：确认 Windows SDK 与 C++/WinRT 组件安装完整。

### Qt Multimedia / Qt Test 相关

- 确保 Qt6 安装时勾选了 **Qt Multimedia** 模块。
- 若需运行 `tests/` 下的 QtTest，可同时安装 **Qt Test** 模块。
- `WASAPI` 系统声音采集仅在 Windows 10/11 可用；其它平台不会采集到系统声音数据。

### `ctest` 不是内部或外部命令

- `ctest` 通常位于 `C:\Qt\Tools\CMake_64\bin\`；如果不在 `PATH` 中，请直接使用绝对路径运行。
- 也可以先在当前终端执行 `set PATH=C:\Qt\Tools\CMake_64\bin;%PATH%`，再运行 `ctest --test-dir build -C Debug --output-on-failure`。
- `test_capture_smoke_windows` 带 `Manual` label，需要通过 `ctest --test-dir build -C Debug -L Manual -V` 单独触发。
- 如果暂时不使用 `ctest`，也可以直接运行 `build\tests\Debug\test_*.exe`。

### Linux / macOS

只有 `GrabWindow`（`QScreen::grabWindow`）采集路径可用；`DXGI` / `WGC` / `WASAPI` 相关代码通过 `#ifdef Q_OS_WIN` / `#ifdef WIN32` 隔离，不影响编译。音频功能依赖系统 PulseAudio / CoreAudio 设备。

---

## 相关文档

| 文档 | 说明 |
|------|------|
| [../README.md](../README.md) | 查看项目概览、目录结构与测试入口 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | 查看线程模型、sender thread 与析构顺序 |
| [TEST_CHECKLIST.md](TEST_CHECKLIST.md) | 查看自动化测试与手动验收清单 |
