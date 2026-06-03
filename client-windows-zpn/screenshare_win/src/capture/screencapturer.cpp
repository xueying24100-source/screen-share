#include "screencapturer.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QColor>
#include <QDebug>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#endif

// ============================================================================
// Screen enumeration
// ============================================================================

QList<QScreen*> ScreenCapturer::screens()
{
    return QGuiApplication::screens();
}

QScreen* ScreenCapturer::primaryScreen()
{
    return QGuiApplication::primaryScreen();
}

int ScreenCapturer::screenCount()
{
    return QGuiApplication::screens().size();
}

// ============================================================================
// Utility
// ============================================================================

bool ScreenCapturer::imageLooksMostlyBlack(const QImage& imageIn)
{
    if (imageIn.isNull()) {
        return true;
    }

    const QImage image = (imageIn.format() == QImage::Format_RGB32)
                             ? imageIn
                             : imageIn.convertToFormat(QImage::Format_RGB32);
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
            const QColor color = image.pixelColor(x, y);
            const int brightness = std::max({color.red(), color.green(), color.blue()});
            if (brightness < 30) {
                ++blackCount;
            }
        }
    }

    return (static_cast<double>(blackCount) / static_cast<double>(totalSamples)) > 0.92;
}

// ============================================================================
// Screen capture - DXGI preferred
// ============================================================================

QImage ScreenCapturer::captureScreenOnce(int screenIndex, const QSize& outputSize)
{
    const QList<QScreen*> screenList = screens();
    if (screenIndex < 0 || screenIndex >= screenList.size()) {
        qWarning() << "[ScreenCapturer] Invalid screen index:" << screenIndex;
        return {};
    }

#ifdef Q_OS_WIN
    // Try DXGI first for best performance
    QImage dxgiFrame = captureScreenWithDXGI(screenIndex, outputSize);
    if (!dxgiFrame.isNull() && !imageLooksMostlyBlack(dxgiFrame)) {
        return dxgiFrame;
    }
#endif

    // Fallback to Qt grabWindow
    QScreen* screen = screenList.at(screenIndex);
    if (!screen) {
        return {};
    }

    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        return {};
    }

    QImage result = pixmap.toImage();
    if (outputSize.isValid()) {
        result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return result.convertToFormat(QImage::Format_RGB32);
}

#ifdef Q_OS_WIN
QImage ScreenCapturer::captureScreenWithDXGI(int screenIndex, const QSize& outputSize)
{
    using namespace Microsoft::WRL;

    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    ComPtr<IDXGIOutputDuplication> duplication;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels,
        static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
        D3D11_SDK_VERSION,
        d3dDevice.ReleaseAndGetAddressOf(),
        &featureLevel,
        d3dContext.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr)) {
        return {};
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        return {};
    }

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(static_cast<UINT>(screenIndex), output.GetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
        return {};
    }

    hr = output1->DuplicateOutput(d3dDevice.Get(), duplication.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> desktopResource;
    hr = duplication->AcquireNextFrame(0, &frameInfo, desktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return {};
    }
    if (FAILED(hr)) {
        return {};
    }

    ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) {
        duplication->ReleaseFrame();
        return {};
    }

    D3D11_TEXTURE2D_DESC desc{};
    desktopTexture->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0) {
        duplication->ReleaseFrame();
        return {};
    }

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    stagingDesc.ArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = d3dDevice->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        duplication->ReleaseFrame();
        return {};
    }

    d3dContext->CopyResource(stagingTexture.Get(), desktopTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = d3dContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        duplication->ReleaseFrame();
        return {};
    }

    QImage frame(static_cast<int>(desc.Width), static_cast<int>(desc.Height), QImage::Format_RGB32);
    if (!frame.isNull()) {
        const int srcBytesPerLine = static_cast<int>(mapped.RowPitch);
        const int dstBytesPerLine = frame.bytesPerLine();
        const int copyBytes = std::min(srcBytesPerLine, dstBytesPerLine);
        const auto* src = static_cast<const unsigned char*>(mapped.pData);
        for (int y = 0; y < frame.height(); ++y) {
            std::memcpy(frame.scanLine(y), src + (static_cast<size_t>(y) * mapped.RowPitch), static_cast<size_t>(copyBytes));
        }
    }

    d3dContext->Unmap(stagingTexture.Get(), 0);
    duplication->ReleaseFrame();

    if (outputSize.isValid()) {
        frame = frame.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return frame;
}
#endif

// ============================================================================
// Window capture - WGC preferred, then GDI
// ============================================================================

QImage ScreenCapturer::captureWindowOnce(quintptr windowId, const QSize& outputSize)
{
    if (windowId == 0) {
        return {};
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!IsWindow(hwnd)) {
        return {};
    }

    // Skip minimized windows
    if (IsIconic(hwnd)) {
        return {};
    }

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return {};
    }
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    if (width <= 0 || height <= 0) {
        return {};
    }

    // Try WGC first (Windows Graphics Capture) - best for hardware accelerated windows
    // Note: WGC requires setting up a session, for one-shot we use GDI approaches

    // Try PrintWindow with PW_RENDERFULLCONTENT first
    QImage result = captureWindowWithPrintWindow(hwnd, width, height);
    if (!result.isNull() && !imageLooksMostlyBlack(result)) {
        if (outputSize.isValid()) {
            result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return result.convertToFormat(QImage::Format_RGB32);
    }

    // Try BitBlt as fallback
    result = captureWindowWithBitBlt(hwnd, width, height);
    if (!result.isNull() && !imageLooksMostlyBlack(result)) {
        if (outputSize.isValid()) {
            result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return result.convertToFormat(QImage::Format_RGB32);
    }

    // Last resort: screen crop
    result = captureWindowFromScreen(hwnd, windowRect);
    if (!result.isNull() && !imageLooksMostlyBlack(result)) {
        if (outputSize.isValid()) {
            result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return result.convertToFormat(QImage::Format_RGB32);
    }

    return {};
#else
    Q_UNUSED(outputSize);
    return {};
#endif
}

#ifdef Q_OS_WIN
QImage ScreenCapturer::captureWindowWithPrintWindow(HWND hwnd, int width, int height)
{
    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return {};

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return {};
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return {};
    }

    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, hBitmap));

    BOOL success = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);

    QImage result;
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

    return result;
}

QImage ScreenCapturer::captureWindowWithBitBlt(HWND hwnd, int width, int height)
{
    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return {};

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return {};
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return {};
    }

    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, hBitmap));

    HDC hdcWindow = GetWindowDC(hwnd);
    BOOL success = FALSE;
    if (hdcWindow) {
        success = BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY | CAPTUREBLT);
        ReleaseDC(hwnd, hdcWindow);
    }

    QImage result;
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

    return result;
}

QImage ScreenCapturer::captureWindowFromScreen(HWND hwnd, const RECT& windowRect)
{
    Q_UNUSED(hwnd);

    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;

    QPoint topLeft(windowRect.left, windowRect.top);
    QScreen* screen = QGuiApplication::screenAt(topLeft);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return {};
    }

    QPixmap fullScreen = screen->grabWindow(0);
    QRect windowArea(windowRect.left, windowRect.top, width, height);
    QPixmap windowPix = fullScreen.copy(windowArea);

    return windowPix.toImage();
}
#endif
