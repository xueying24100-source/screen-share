#include "screencapturer.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace {
QImage prepareOutputFrame(const QImage &frame, const QSize &outputSize)
{
    if (frame.isNull()) {
        return {};
    }

    QImage output = frame;
    if (outputSize.isValid()
        && (frame.width() > outputSize.width() || frame.height() > outputSize.height())) {
        output = frame.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return output.convertToFormat(QImage::Format_RGB32);
}

#ifdef Q_OS_MACOS
QImage imageFromCGImage(CGImageRef imageRef)
{
    if (!imageRef) {
        return {};
    }

    const int width = static_cast<int>(CGImageGetWidth(imageRef));
    const int height = static_cast<int>(CGImageGetHeight(imageRef));
    if (width <= 0 || height <= 0) {
        return {};
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return {};
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        image.bits(),
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        8,
        static_cast<size_t>(image.bytesPerLine()),
        colorSpace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

    if (colorSpace) {
        CGColorSpaceRelease(colorSpace);
    }
    if (!context) {
        return {};
    }

    CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);
    CGContextRelease(context);
    return image;
}
#endif
}

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ScreenCapturer::captureFrame);
}

void ScreenCapturer::start(int fps)
{
    if (m_running) {
        return;
    }

    const int interval = (fps > 0) ? qMax(16, 1000 / fps) : 66;
    m_frameIndex = 0;
    m_timer->start(interval);
    m_running = true;
}

void ScreenCapturer::startScreen(int screenIndex, int fps)
{
    if (m_running) {
        stop();
    }

    m_captureMode = CaptureMode::IndexedScreen;
    m_screenIndex = screenIndex;
    m_windowHandle = 0;
    start(fps);
}

void ScreenCapturer::startWindow(quintptr windowId, int fps)
{
    if (m_running) {
        stop();
    }

    if (windowId == 0) {
        emit captureError("Invalid window handle");
        return;
    }

    m_captureMode = CaptureMode::Window;
    m_windowHandle = windowId;
    start(fps);
}

void ScreenCapturer::stop()
{
    if (!m_running) {
        return;
    }

    m_timer->stop();
    m_running = false;
    m_frameIndex = 0;
}

void ScreenCapturer::captureFrame()
{
    if (m_captureMode == CaptureMode::Window) {
        captureWindow();
    } else {
        captureScreen();
    }
}

void ScreenCapturer::captureScreen()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    QScreen *screen = nullptr;

    if (m_captureMode == CaptureMode::PrimaryScreen) {
        screen = QGuiApplication::primaryScreen();
    } else if (m_screenIndex >= 0 && m_screenIndex < screens.size()) {
        screen = screens.at(m_screenIndex);
    }

    if (!screen) {
        emit captureError("Screen not available");
        return;
    }

    const QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        emit captureError("grabWindow() returned null pixmap");
        return;
    }

    emitFrame(prepareOutputFrame(pixmap.toImage(), m_outputSize),
              "GrabWindow",
              pixmap.size(),
              screen->geometry());
}

void ScreenCapturer::captureWindow()
{
#ifdef Q_OS_MACOS
    const CGWindowID windowId = static_cast<CGWindowID>(m_windowHandle);
    CGImageRef imageRef = CGWindowListCreateImage(
        CGRectNull,
        kCGWindowListOptionIncludingWindow,
        windowId,
        kCGWindowImageBoundsIgnoreFraming | kCGWindowImageNominalResolution);

    if (!imageRef) {
        emit captureError("无法捕获该窗口，请检查屏幕录制权限或窗口是否仍然存在");
        return;
    }

    const QImage rawFrame = imageFromCGImage(imageRef);
    CGImageRelease(imageRef);

    if (rawFrame.isNull()) {
        emit captureError("Window frame conversion failed");
        return;
    }

    emitFrame(prepareOutputFrame(rawFrame, m_outputSize),
              "CGWindowListCreateImage",
              rawFrame.size());
#else
    emit captureError("Window capture is only implemented on macOS");
#endif
}

void ScreenCapturer::emitFrame(const QImage &frame, const QString &backendName,
                              const QSize &sourceSize, const QRect &sourceGeometry)
{
    if (frame.isNull()) {
        emit captureError("Captured frame is empty");
        return;
    }

    ++m_frameIndex;
    emit frameCaptured(frame);

    CaptureFrameMetadata meta;
    meta.sourceSize = sourceSize;
    meta.sourceGeometry = sourceGeometry;
    meta.windowHandle = (m_captureMode == CaptureMode::Window) ? m_windowHandle : 0;
    meta.backendName = backendName;
    meta.frameIndex = m_frameIndex;
    emit frameMetadataChanged(meta);
}

