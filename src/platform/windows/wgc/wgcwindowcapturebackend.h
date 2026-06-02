#pragma once
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <QImage>
#include <QSize>
#include <memory>

struct HWND__;
typedef HWND__* HWND;

class WgcWindowCaptureBackend {
public:
    WgcWindowCaptureBackend();
    ~WgcWindowCaptureBackend();

    static bool isSupported();
    bool start(HWND hwnd);
    void stop();
    bool isRunning() const;
    void setCursorCaptureEnabled(bool enabled);
    void setBorderRequired(bool required);
    void setMinUpdateInterval(int ms);

    QImage tryGetFrame();

    QSize lastFrameSize() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif
