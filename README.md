# Screen Share - 窗口枚举模块

## 项目简介

本项目是屏幕共享软件的 **窗口枚举与显示** 模块，负责：

- 枚举 Windows 系统中所有可见窗口
- 过滤系统窗口（桌面、任务栏等）
- 在 UI 列表中显示窗口标题
- 获取用户选中的窗口句柄（HWND）
- 将窗口句柄传递给采集模块进行屏幕共享

## 开发环境

| 工具 | 版本 |
|------|------|
| 操作系统 | Windows 11 |
| 开发框架 | Qt 6.11.1 |
| 编译器 | MSVC 2022 64-bit |
| 版本控制 | Git |

## 目录结构
```



screen-share/
├── src/
│ ├── app/
│ │ └── main.cpp # 程序入口
│ └── ui/
│ └── picker/
│ ├── windowpicker.h # 窗口选择器头文件
│ ├── windowpicker.cpp # 窗口选择器实现
│ └── windowpicker.ui # 窗口选择器界面
├── CMakeLists.txt # CMake 构建配置
└── README.md # 项目说明