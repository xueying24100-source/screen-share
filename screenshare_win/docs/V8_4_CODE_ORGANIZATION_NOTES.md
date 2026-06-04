# V8.4 代码整理分块说明

本版本只做代码结构整理，不改动现有功能逻辑、UI 风格和网络流程。

## 主要改动

`MainWindow` 原来主要集中在一个 `mainwindow.cpp` 中。为了方便同学继续维护，本版本把 `MainWindow` 的成员函数按职责拆成多个实现文件：

| 文件 | 职责 |
|---|---|
| `mainwindow.cpp` | 构造、析构、基础初始化 |
| `mainwindow_share_popup.cpp` | 共享选择弹窗、桌面/窗口缩略图、窗口枚举、状态文字 |
| `mainwindow_toolbar.cpp` | 共享悬浮工具条、画笔开关同步 |
| `mainwindow_meeting.cpp` | 创建会议、加入会议、会议码、多人网络事件 |
| `mainwindow_participants.cpp` | 动态参会者小窗口、远端接收器管理 |
| `mainwindow_camera_media.cpp` | 摄像头、本地/远端媒体接收、Sender 启停 |
| `mainwindow_share_logic.cpp` | 开始共享、结束共享、桌面/窗口/白板共享、定时抓屏 |
| `mainwindow_annotation.cpp` | 画笔窗口、白板预览、批注合成、窗口采集辅助 |
| `mainwindow_impl_includes.h` | MainWindow 实现文件共用 include，避免每个 cpp 重复维护头文件 |

## 未改动的内容

- 没改 UI 控件命名。
- 没改会议码逻辑。
- 没改多人网络转发逻辑。
- 没改摄像头、共享、画笔、音频的行为。
- 没改普通模式 PNG 高清、流畅模式 JPEG 的策略。

## 为什么这样拆

这属于低风险重构：类定义仍然在 `mainwindow.h`，只是把实现按功能拆到不同 `.cpp` 文件中。
这样可以保持原有运行逻辑，同时降低单个文件长度，方便按模块定位问题。

## 后续可继续优化

如果后续还要继续工程化，可以再把 `MainWindow` 中的部分逻辑抽成真正独立类：

- `MeetingController`：会议创建、加入、会议码、网络状态。
- `ShareController`：共享源选择、共享开始/结束、抓屏。
- `ParticipantPanel` / `ParticipantTile`：动态参会者 UI。
- `MeetingServer` / `MeetingClient`：进一步拆分 `networktransport.cpp`。

当前版本先不做这些抽类，避免影响已验证功能。
