# V8.1 UI 同步说明

本版本主要修复 Qt Designer 中 `mainwindow.ui` 与实际运行界面不一致的问题。

## 修改内容

1. `mainwindow.ui` 中直接加入底部控制按钮：
   - `btnHostMeeting`：创建会议
   - `btnConnectLocal`：加入会议
   - `btnShare`：开始共享
   - `btnAnnotate`：画笔
   - `btnEnd`：结束共享
   - `btnCamera`：打开摄像头 / 关闭摄像头

2. 代码中不再动态 `new QPushButton` 创建会议相关按钮。
   - `MainWindow::setupMeetingControls()` 现在只从 `ui` 中读取按钮指针。
   - 这样 Qt Designer 里看到的界面和运行时界面一致。

3. 顶部参会者区域改入 `mainwindow.ui`：
   - `participantScrollArea`
   - `participantContainer`
   - `participantLayout`

4. 运行时仍然保持动态参会者逻辑：
   - 创建会议 / 加入会议后自动添加用户卡片。
   - 摄像头开启后在对应用户卡片里显示画面。
   - 摄像头关闭后恢复占位文字。

## 保留内容

- 原来的蓝色、绿色、红色、浅灰色按钮风格保持不变。
- 多人会议网络逻辑不变。
- 会议码逻辑不变。
- 摄像头中文错误提示不变。
