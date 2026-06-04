#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QImage>
#include <QRect>
#include <QSize>
#include <QList>

class QScreen;

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

class ScreenCapturer
{
public:
    // One-shot capture methods
    // Capture screen by index, prefers DXGI on Windows
    static QImage captureScreenOnce(int screenIndex, const QSize& outputSize = QSize());

    // Capture window by handle, prefers WGC/PrintWindow/BitBlt on Windows
    static QImage captureWindowOnce(quintptr windowId, const QSize& outputSize = QSize());

    // Utility
    static bool imageLooksMostlyBlack(const QImage& image);

#ifdef Q_OS_WIN
private:
    // DXGI screen capture
    static QImage captureScreenWithDXGI(int screenIndex, const QSize& outputSize);

    // WGC window capture (Windows Graphics Capture - hardware accelerated)
    static QImage captureWindowWithWGC(HWND hwnd, const QSize& outputSize);

    // GDI window capture methods
    static QImage captureWindowWithPrintWindow(HWND hwnd, int width, int height);
    static QImage captureWindowWithBitBlt(HWND hwnd, int width, int height);
    static QImage captureWindowFromScreen(HWND hwnd, const RECT& windowRect);
#endif
};
