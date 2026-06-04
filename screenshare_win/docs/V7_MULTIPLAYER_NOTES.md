# V7 多人会议改造说明

本版本在 v6 架构分层版基础上继续改造，目标是从“本机双开点对点 Demo”升级为“本机多开多人会议 Demo”。

## 主要变化

### 1. 网络层改为多人中心转发

原来：

```text
A <-> B
```

现在：

```text
        B
        |
C ----  A(host/server) ---- D
        |
        E
```

同一个 exe 既可以创建会议，也可以加入会议：

- 点击“创建会议”：当前 exe 成为 Host / Server，同时也是本地参会者。
- 点击“加入会议”：当前 exe 连接 `127.0.0.1:9000`，成为 Client。

网络实现仍在：

```text
src/network/networktransport.h
src/network/networktransport.cpp
```

但内部已经从单个 `QTcpSocket` 改为：

```text
QTcpServer + 多个 QTcpSocket client session
```

每个媒体包外层会增加会议信封，携带：

```text
EnvelopeType
senderId
userName
payload
```

这样 UI 可以知道每路摄像头 / 共享画面来自哪个用户。

---

### 2. 顶部小窗口改为动态参会者栏

原来 UI 固定 5 个 QLabel：

```text
用户1 用户2 用户3 用户4 用户5
```

现在改成动态容器：

```text
QScrollArea + QHBoxLayout + 动态 QLabel tile
```

逻辑在：

```text
src/app/mainwindow.cpp
initializeParticipantPanel()
ensureParticipantTile()
removeParticipantTile()
updateParticipantVideo()
```

效果：

- 创建会议后显示主机本人的小窗口。
- 每加入一个客户端，就新增一个小窗口。
- 用户离开后自动移除小窗口。
- 摄像头关闭后对应小窗口恢复占位文字。

---

### 3. 多人摄像头分发

每个用户打开摄像头后：

```text
CameraManager -> Sender -> TcpPacketTransport -> Host 转发 -> 其他客户端 MediaReceiver -> 对应用户小窗口
```

Host 也会在本地显示每个客户端的摄像头。

---

### 4. 共享屏幕转发

任意用户开始共享后：

```text
共享帧 -> Sender -> TcpPacketTransport -> Host 转发 -> 其他客户端大窗口
```

当前规则：

- 自己正在共享时，大窗口优先显示自己的共享内容。
- 自己没有共享时，大窗口显示远端共享内容。
- 如果多人同时共享，接收端会显示最近收到的一路共享画面。后续可以扩展为“同一时刻只允许一个人共享”。

---

### 5. 音频基础接入

远端音频包已经接入到每个远端 `MediaReceiver`，收到后会进入 `AudioPlayer` 播放。

当前仍是基础 Demo 级音频链路，后续可以继续优化：

- 多路混音
- 回声消除
- 静音状态同步
- 音视频同步

---

## 测试流程

### 本机三开测试

1. 打开第一个 exe，点击“创建会议”。
2. 打开第二个 exe，点击“加入会议”。
3. 打开第三个 exe，点击“加入会议”。
4. 第二个和第三个 exe 分别点击“打开摄像头”。
5. 第一个 exe 顶部应出现两个远端摄像头小窗口。
6. 第一个 exe 点击“开始共享 -> 桌面1”。
7. 第二个和第三个 exe 的大窗口应显示第一个 exe 的共享屏幕。

### 推荐演示顺序

```text
A：创建会议
B：加入会议
C：加入会议
B：打开摄像头
C：打开摄像头
A：开始共享桌面1
```

预期：

```text
A 顶部：B 摄像头 + C 摄像头
A 大屏：A 自己共享的桌面
B 顶部：B 自己摄像头 + C 摄像头
B 大屏：A 共享桌面
C 顶部：B 摄像头 + C 自己摄像头
C 大屏：A 共享桌面
```

---

## 已删除的旧模块

旧版残留的独立 Sender/Receiver UI 已从构建中移除：

```text
src/app/senderwindow.*
src/app/receiverwindow.*
```

当前发送 / 接收能力由以下模块负责：

```text
src/media/sender.*
src/media/mediareceiver.*
src/network/networktransport.*
```
