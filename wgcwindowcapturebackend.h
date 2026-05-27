#pragma once
#ifdef Q_OS_WIN
#include <QImage>
#include <QSize>
#include <memory>
#include <Windows.h>

class WgcWindowCaptureBackend {
public:
    WgcWindowCaptureBackend();
    ~WgcWindowCaptureBackend();

    static bool isSupported();     // 检查 Win10 1803+ API 可用性
    bool start(HWND hwnd);         // 初始化 capture item + frame pool + session
    void stop();                   // 释放所有 WinRT/D3D 资源
    bool isRunning() const;

    // 在 timer tick 时调用，返回最新帧（null 表示无新帧）
    QImage tryGetFrame();

    // 最近一帧的原始尺寸（用于 metadata）
    QSize lastFrameSize() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // Q_OS_WIN
