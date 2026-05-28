# 局域网屏幕共享软件

基于 **C++ / Qt/ WebRtc** 实现的局域网屏幕共享工具，支持 Windows 和 macOS。

---

## 分工与进度

| 模块 | 平台 | 负责人 | 分支 | 职责 |
|------|------|--------|------|------|
| 屏幕共享模块 | Windows | 蒋宗原 | `jzy` | 窗口枚举与显示 |
| 屏幕共享模块 | Windows | 韦燕丹 | `wyd` | 屏幕采集 |
| 屏幕共享模块 | macOS | 俞哲钊 | `yzz` | 窗口枚举与显示 |
| 屏幕共享模块 | macOS | 邢雨茁 | `xyz` | 屏幕采集 |
| 客户端业务架构 | macOS | 黄俊杰 | `hjj` | 客户端主体框架 |
| 客户端业务架构 | Windows | 赵芃年 | `zpn` | 客户端主体框架 |

---

## Milestone

### Week 1
- 开发环境搭建（Qt 6.5 + libwebrtc 预编译包）
- 实现窗口或屏幕枚举（能列出当前系统所有窗口/屏幕）
- 在自己的分支上完成初步开发

### Week 2
- 实现屏幕或窗口的实际采集（抓到画面内容）
- 与客户端同学进行联调
- 完成功能验收

---

## Git 工作流

```bash
# 1. 克隆仓库
git clone https://github.com/xueying24100-source/screen-share.git

# 2. 切换到自己的分支（换成自己的分支名）
git checkout jzy

# 3. 开发完成后提交
git add .
git commit -m "feat: 实现屏幕枚举"
git push origin jzy
```

> 每人只在自己的分支开发，不要直接提交到 `main`。

