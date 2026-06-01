#include "screencapturer.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDebug>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
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

}

bool ScreenCapturer::imageLooksMostlyBlack(const QImage &imageIn)
{
    if (imageIn.isNull()) {
        return true;
    }

    QImage image = imageIn;
    if (image.format() != QImage::Format_RGB32) {
        image = image.convertToFormat(QImage::Format_RGB32);
    }
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return true;
    }

    const int sampleCols = std::min(80, image.width());
    const int sampleRows = std::min(80, image.height());
    const int totalSamples = sampleCols * sampleRows;
    if (totalSamples <= 0) {
        return true;
    }

    int blackCount = 0;
    for (int row = 0; row < sampleRows; ++row) {
        const int y = (sampleRows == 1) ? 0 : (row * (image.height() - 1) / (sampleRows - 1));
        for (int col = 0; col < sampleCols; ++col) {
            const int x = (sampleCols == 1) ? 0 : (col * (image.width() - 1) / (sampleCols - 1));
            const QRgb pixel = image.pixel(x, y);
            const int brightness = qMax(qRed(pixel), qMax(qGreen(pixel), qBlue(pixel)));
            if (brightness < 30) {
                ++blackCount;
            }
        }
    }
    return (static_cast<double>(blackCount) / static_cast<double>(totalSamples)) > 0.92;
}

QImage ScreenCapturer::captureWindowOnce(quintptr windowId, const QSize &outputSize)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd || !IsWindow(hwnd)) {
        return {};
    }
    // if (IsIconic(hwnd)) {
    //     return {};
    // }

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return {};
    }
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    if (width <= 0 || height <= 0) {
        return {};
    }

    QImage result;

    // Method 1: PrintWindow
    {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

        BOOL success = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);

        if (!success) {
            HDC hdcWindow = GetWindowDC(hwnd);
            if (hdcWindow) {
                success = BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY | CAPTUREBLT);
                ReleaseDC(hwnd, hdcWindow);
            }
        }

        if (success) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = width;
            bmi.bmiHeader.biHeight = -height;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            result = QImage(width, height, QImage::Format_ARGB32);
            if (!result.isNull()) {
                GetDIBits(hdcMem, hBitmap, 0, height, result.bits(), &bmi, DIB_RGB_COLORS);
            }
        }

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    // Method 2: Screen crop fallback
    if (result.isNull() || imageLooksMostlyBlack(result)) {
        QPoint topLeft(windowRect.left, windowRect.top);
        QRect windowArea(windowRect.left, windowRect.top, width, height);

        QScreen* screen = QGuiApplication::screenAt(topLeft);
        if (!screen) screen = QGuiApplication::primaryScreen();

        QPixmap fullScreen = screen->grabWindow(0);
        QPixmap windowPix = fullScreen.copy(windowArea);
        result = windowPix.toImage();
    }

    if (result.isNull() || result.width() <= 0 || imageLooksMostlyBlack(result)) {
        return {};
    }

    if (outputSize.isValid()) {
        result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return result.convertToFormat(QImage::Format_RGB32);
#else
    return {};
#endif
}

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ScreenCapturer::captureFrame);
}

ScreenCapturer::~ScreenCapturer()
{
    stop();
}

void ScreenCapturer::startWindow(quintptr windowId, int fps)
{
    if (m_running) {
        stop();
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd || !IsWindow(hwnd)) {
        emit captureError("Invalid window handle");
        return;
    }
    if (IsIconic(hwnd)) {
        emit captureError("Window is minimized, cannot capture");
        return;
    }
#endif

    m_windowHandle = windowId;

    const int interval = (fps > 0) ? qMax(16, 1000 / fps) : 66;
    m_frameIndex = 0;
    m_timer->start(interval);
    m_running = true;
    qDebug() << "[ScreenCapturer] started, interval:" << interval << "ms";
}

void ScreenCapturer::stop()
{
    if (!m_running) {
        return;
    }

    m_timer->stop();
    m_running = false;
    m_frameIndex = 0;
    qDebug() << "[ScreenCapturer] stopped";
}

void ScreenCapturer::captureFrame()
{
    captureWindow();
}

void ScreenCapturer::captureWindow()
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_windowHandle);
    if (!hwnd || !IsWindow(hwnd)) {
        emit captureError("Invalid window handle");
        return;
    }
    if (IsIconic(hwnd)) {
        return;
    }

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        emit captureError("GetWindowRect failed");
        return;
    }
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    if (width <= 0 || height <= 0) {
        emit captureError("Window has invalid size");
        return;
    }

    QImage result;
    QString backendName;

    {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

        BOOL success = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);
        backendName = "PrintWindow";

        if (!success) {
            HDC hdcWindow = GetWindowDC(hwnd);
            if (hdcWindow) {
                success = BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY | CAPTUREBLT);
                ReleaseDC(hwnd, hdcWindow);
                backendName = "BitBlt";
            }
        }

        if (success) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = width;
            bmi.bmiHeader.biHeight = -height;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            result = QImage(width, height, QImage::Format_ARGB32);
            if (!result.isNull()) {
                GetDIBits(hdcMem, hBitmap, 0, height, result.bits(), &bmi, DIB_RGB_COLORS);
            }
        }

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    if (result.isNull() || result.width() <= 0 || imageLooksMostlyBlack(result)) {
        return;
    }

    const QImage frame = prepareOutputFrame(result, m_outputSize);
    ++m_frameIndex;
    emit frameCaptured(frame);

    CaptureFrameMetadata meta;
    meta.sourceSize = result.size();
    meta.sourceGeometry = QRect(windowRect.left, windowRect.top, width, height);
    meta.windowHandle = m_windowHandle;
    meta.backendName = backendName;
    meta.frameIndex = m_frameIndex;
    emit frameMetadataChanged(meta);
#else
    emit captureError("Window capture only implemented on Windows");
#endif
}
