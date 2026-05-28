# 构建说明

## 环境要求

| 依赖 | 版本要求 | 说明 |
|------|---------|------|
| **Qt** | 6.x | Core、Gui、Widgets、Multimedia 四个模块均需安装 |
| **CMake** | ≥ 3.16 | 项目构建系统 |
| **编译器** | C++20 | Windows: MSVC 2022（推荐）；Linux/macOS: GCC 12+ / Clang 14+ |
| **操作系统** | Windows 10/11（完整功能）| macOS / Linux 仅支持 `GrabWindow` 采集路径，无 DXGI/WGC/WASAPI |
| **Windows SDK** | 10.0.19041.0+（Win10 2004）| 需包含 WinRT 头文件（`winrt/base.h`、`windows.graphics.capture.h`）以支持 WGC 后端 |
| **C++/WinRT** | 随 Windows SDK 附带 | WGC 后端通过 C++/WinRT 投影访问 Windows.Graphics.Capture API |

> WGC（Windows Graphics Capture）窗口采集功能需要 Windows 10 1803（RS4）及以上版本在**运行时**可用，构建时只需 SDK 版本满足要求即可。

---

## 依赖库（Windows 平台）

`CMakeLists.txt` 在 Windows 下自动链接以下系统库：

| 库 | 用途 |
|----|------|
| `d3d11` | DXGI Desktop Duplication（全屏采集）|
| `dxgi` | DXGI 接口（配合 d3d11）|
| `user32` | 窗口操作（`IsWindow`、`PrintWindow` 等）|
| `windowsapp` | C++/WinRT 运行时（WGC 后端）|
| `ole32` | COM 初始化 |
| `mmdevapi` | WASAPI 设备枚举与 loopback 采集 |
| `propsys` | 音频设备属性（`WASAPI`）|
| `avrt` | 音频实时线程优先级（`AvSetMmThreadCharacteristics`）|

---

## CMake 关键配置

`CMakeLists.txt` 片段（供参考）：

```cmake
cmake_minimum_required(VERSION 3.16)
project(ScreenShare_Capturer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # Qt moc 自动处理

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

# Windows 编译选项
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00      # 目标 Windows 10
    )
    target_compile_options(${PROJECT_NAME} PRIVATE
        /await:strict            # 启用 C++/WinRT 协程支持
        /Zc:__cplusplus          # 修复 MSVC 的 __cplusplus 宏
    )
endif()
```

- 编译产物名为 **`ScreenShare_Capturer`**（由 `project()` 名称决定）。
- 所有源文件在 `CMakeLists.txt` 的 `SOURCES` 列表中统一管理，新增源文件需同步更新该列表。

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
# Release 构建（推荐）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 或 Debug 构建（含调试信息）
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

如果 Qt6 未在系统路径中，需手动指定 Qt 安装目录：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
```

### 3. 编译

```bash
cmake --build build --parallel
```

### 4. 运行

```
build/Release/ScreenShare_Capturer.exe   （Windows MSVC）
build/ScreenShare_Capturer               （Linux/macOS）
```

---

## Qt Creator 集成

项目根目录含 `.qtcreator/` 配置，可直接在 Qt Creator 中通过「打开项目」选择 `CMakeLists.txt` 导入。

---

## 常见问题

### WGC 相关编译错误

- **`winrt/base.h` 找不到**：确保已安装 Windows SDK 10.0.19041.0+，并在 CMake 中正确配置 MSVC 工具链。
- **`/await:strict` 不支持**：需使用 MSVC 2019 16.8 或更高版本（Visual Studio 2022 推荐）。
- **`windowsapp.lib` 链接失败**：确认 Windows SDK 版本和 C++/WinRT NuGet 包已安装。

### Qt Multimedia 相关

- 确保 Qt6 安装时勾选了 **Qt Multimedia** 模块；部分 Qt 在线安装器默认不勾选该模块。
- WASAPI 系统声音采集在 Windows 10/11 外的平台不可用，`SystemAudioCapturer` 不会采集到任何数据。

### Linux / macOS

只有 `GrabWindow`（`QScreen::grabWindow`）采集路径可用，DXGI/WGC/WASAPI 相关代码通过 `#ifdef Q_OS_WIN` / `#ifdef WIN32` 隔离，不影响编译。音频功能依赖系统 PulseAudio / CoreAudio 设备。
