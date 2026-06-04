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
#define WINRT_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

// WGC (Windows Graphics Capture) 头文件
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#endif

//判断是否为黑屏
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

//屏幕采集
QImage ScreenCapturer::captureScreenOnce(int screenIndex, const QSize& outputSize)
{
    const QList<QScreen*> screenList = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screenList.size()) {
        qWarning() << "[ScreenCapturer] Invalid screen index:" << screenIndex;
        return {};
    }
    //优先级1：DXGI
    QImage dxgiFrame = captureScreenWithDXGI(screenIndex, outputSize);
    if (!dxgiFrame.isNull() && !imageLooksMostlyBlack(dxgiFrame)) {
        return dxgiFrame;
    }
    //优先级2：Qt grabWindow
    QScreen* screen = screenList.at(screenIndex);
    if (!screen) {
        return {};
    }
    QPixmap pixmap = screen->grabWindow(0);//实际上是BitBlt（GDI）
    if (pixmap.isNull()) {
        return {};
    }
    QImage result = pixmap.toImage();
    if (outputSize.isValid()) {
        result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return result.convertToFormat(QImage::Format_RGB32);
}

// DXGI 屏幕捕获
QImage ScreenCapturer::captureScreenWithDXGI(int screenIndex, const QSize& outputSize)
{
    using namespace Microsoft::WRL;

    // D3D11 设备和上下文，用于 GPU 操作
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    // 桌面副本对象，用于捕获屏幕
    ComPtr<IDXGIOutputDuplication> duplication;

    // 尝试从高到低的特性级别，确保兼容性
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    // 步骤1: 创建 D3D11 设备
    // 这是所有 DirectX 操作的基础，代表一个虚拟 GPU
    HRESULT hr = D3D11CreateDevice(
        nullptr,                        // 默认适配器
        D3D_DRIVER_TYPE_HARDWARE,       // 使用硬件 GPU
        nullptr,                        // 不指定软件模块
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, // 支持 BGRA 格式（与 Qt 兼容）
        featureLevels,                  // 特性级别数组
        static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
        D3D11_SDK_VERSION,
        d3dDevice.ReleaseAndGetAddressOf(),  // 输出：D3D 设备
        &featureLevel,                  // 输出：实际使用的特性级别
        d3dContext.ReleaseAndGetAddressOf()  // 输出：设备上下文
    );
    if (FAILED(hr)) {
        return {};
    }

    // 步骤2: 从 D3D 设备获取 DXGI 设备
    // DXGI 是 DirectX Graphics Infrastructure，负责与硬件交互
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        return {};
    }

    // 步骤3: 获取显示适配器（显卡）
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    // 步骤4: 枚举显示器输出
    // screenIndex=0 是主显示器，1 是第二个显示器，以此类推
    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(static_cast<UINT>(screenIndex), output.GetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    // 步骤5: 获取 IDXGIOutput1 接口（支持 Desktop Duplication）
    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
        return {};
    }

    // 步骤6: 创建桌面副本
    // 这是 Desktop Duplication API 的核心，允许我们复制桌面内容
    hr = output1->DuplicateOutput(d3dDevice.Get(), duplication.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    // 步骤7: 获取下一帧
    // timeout=0 表示不等待，立即返回
    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> desktopResource;
    hr = duplication->AcquireNextFrame(0, &frameInfo, desktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return {};  // 没有新帧，返回空
    }
    if (FAILED(hr)) {
        return {};
    }

    // 步骤8: 将桌面资源转换为纹理
    ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) {
        duplication->ReleaseFrame();  // 必须释放帧，否则后续调用会失败
        return {};
    }

    // 获取桌面纹理的描述信息（宽度、高度、格式等）
    D3D11_TEXTURE2D_DESC desc{};
    desktopTexture->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0) {
        duplication->ReleaseFrame();
        return {};
    }

    // 步骤9: 创建 Staging 纹理
    // Staging 纹理是 CPU 可读的，用于将 GPU 数据拷贝到内存
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;     // Staging 用法，CPU 可读
    stagingDesc.BindFlags = 0;                    // 不绑定到渲染管线
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;  // CPU 读取权限
    stagingDesc.MiscFlags = 0;
    stagingDesc.ArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.SampleDesc.Count = 1;             // 不使用多重采样
    stagingDesc.SampleDesc.Quality = 0;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = d3dDevice->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        duplication->ReleaseFrame();
        return {};
    }

    // 步骤10: 将桌面纹理复制到 Staging 纹理
    // GPU 到 GPU 的复制，非常快
    d3dContext->CopyResource(stagingTexture.Get(), desktopTexture.Get());

    // 步骤11: 映射 Staging 纹理到 CPU 内存
    // 这样我们就可以读取像素数据了
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = d3dContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        duplication->ReleaseFrame();
        return {};
    }

    // 步骤12: 创建 QImage 并复制像素数据
    QImage frame(static_cast<int>(desc.Width), static_cast<int>(desc.Height), QImage::Format_RGB32);
    if (!frame.isNull()) {
        const int srcBytesPerLine = static_cast<int>(mapped.RowPitch);  // GPU 纹理的行字节数（可能有对齐填充）
        const int dstBytesPerLine = frame.bytesPerLine();                // QImage 的行字节数
        const int copyBytes = std::min(srcBytesPerLine, dstBytesPerLine);
        const auto* src = static_cast<const unsigned char*>(mapped.pData);
        // 逐行复制，处理行对齐差异
        for (int y = 0; y < frame.height(); ++y) {
            std::memcpy(frame.scanLine(y), src + (static_cast<size_t>(y) * mapped.RowPitch), static_cast<size_t>(copyBytes));
        }
    }

    // 步骤13: 清理资源
    d3dContext->Unmap(stagingTexture.Get(), 0);  // 解除内存映射
    duplication->ReleaseFrame();                  // 释放桌面帧，让系统继续更新

    // 步骤14: 如果指定了输出尺寸，进行缩放
    if (outputSize.isValid()) {
        frame = frame.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return frame;
}

//采集窗口
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

    //跳过最小化窗口
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

    // 优先级1: WGC
    QImage result = captureWindowWithWGC(hwnd, outputSize);
    if (!result.isNull() && !imageLooksMostlyBlack(result)) {
        return result;
    }

    // 优先级2: PrintWindow
    result = captureWindowWithPrintWindow(hwnd, width, height);
    if (!result.isNull() && !imageLooksMostlyBlack(result)) {
        if (outputSize.isValid()) {
            result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return result.convertToFormat(QImage::Format_RGB32);
    }

    // 优先级3: BitBlt
    result = captureWindowWithBitBlt(hwnd, width, height);
    if (!result.isNull() && !imageLooksMostlyBlack(result)) {
        if (outputSize.isValid()) {
            result = result.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return result.convertToFormat(QImage::Format_RGB32);
    }

    // 优先级4: 屏幕裁剪
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

// WGC 窗口捕获 (Windows Graphics Capture)
QImage ScreenCapturer::captureWindowWithWGC(HWND hwnd, const QSize& outputSize)
{
    using namespace Microsoft::WRL;

    // 检查 WGC 是否支持
    static bool wgcSupported = false;
    static bool wgcChecked = false;
    if (!wgcChecked) {
        try {
            wgcSupported = winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
        } catch (...) {
            wgcSupported = false;
        }
        wgcChecked = true;
    }
    if (!wgcSupported) {
        return {};
    }

    try {
        // 步骤1: 创建 D3D11 设备
        ComPtr<ID3D11Device> d3dDevice;
        ComPtr<ID3D11DeviceContext> d3dContext;

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL featureLevel{};
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

        // 步骤2: 获取 DXGI 设备并转换为 WinRT 设备
        ComPtr<IDXGIDevice> dxgiDevice;
        hr = d3dDevice.As(&dxgiDevice);
        if (FAILED(hr)) {
            return {};
        }

        winrt::com_ptr<::IInspectable> inspectable;
        hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
        if (FAILED(hr)) {
            return {};
        }
        auto winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

        // 步骤3: 从窗口句柄创建 GraphicsCaptureItem
        auto factory = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem{nullptr};
        hr = factory->CreateForWindow(
            hwnd,
            winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
            winrt::put_abi(captureItem));
        if (FAILED(hr)) {
            return {};
        }

        auto size = captureItem.Size();
        if (size.Width <= 0 || size.Height <= 0) {
            return {};
        }

        // 步骤4: 创建 FramePool 和 Session
        auto framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
            winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,  // 缓冲帧数
            size);

        auto session = framePool.CreateCaptureSession(captureItem);

        // 配置会话选项
        try {
            session.IsCursorCaptureEnabled(false);  // 不捕获光标
        } catch (...) {}
        try {
            session.IsBorderRequired(false);  // 不需要边框
        } catch (...) {}

        // 步骤5: 开始捕获
        session.StartCapture();

        // 步骤6: 等待并获取帧
        // 由于 WGC 是异步的，我们需要等待一下让帧准备好
        Sleep(50);  // 等待 50ms

        auto frame = framePool.TryGetNextFrame();
        if (!frame) {
            // 再等待一次
            Sleep(100);
            frame = framePool.TryGetNextFrame();
        }

        // 步骤7: 关闭会话
        try { session.Close(); } catch (...) {}
        try { framePool.Close(); } catch (...) {}

        if (!frame) {
            return {};
        }

        // 步骤8: 从帧获取纹理
        auto surface = frame.Surface();
        auto dxgiAccess = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ComPtr<ID3D11Texture2D> frameTex;
        hr = dxgiAccess->GetInterface(IID_PPV_ARGS(frameTex.GetAddressOf()));
        if (FAILED(hr)) {
            try { frame.Close(); } catch (...) {}
            return {};
        }

        D3D11_TEXTURE2D_DESC desc{};
        frameTex->GetDesc(&desc);
        if (desc.Width == 0 || desc.Height == 0) {
            try { frame.Close(); } catch (...) {}
            return {};
        }

        // 步骤9: 创建 Staging 纹理并复制
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
            try { frame.Close(); } catch (...) {}
            return {};
        }

        d3dContext->CopyResource(stagingTexture.Get(), frameTex.Get());

        // 步骤10: 映射到 CPU 内存并创建 QImage
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = d3dContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            try { frame.Close(); } catch (...) {}
            return {};
        }

        QImage image(static_cast<int>(desc.Width), static_cast<int>(desc.Height), QImage::Format_RGB32);
        if (!image.isNull()) {
            const int dstBytesPerLine = image.bytesPerLine();
            const int copyBytes = std::min(static_cast<int>(mapped.RowPitch), dstBytesPerLine);
            const auto* src = static_cast<const unsigned char*>(mapped.pData);
            for (int y = 0; y < image.height(); ++y) {
                std::memcpy(image.scanLine(y), src + static_cast<size_t>(y) * mapped.RowPitch, static_cast<size_t>(copyBytes));
            }
        }

        d3dContext->Unmap(stagingTexture.Get(), 0);
        try { frame.Close(); } catch (...) {}

        // 步骤11: 缩放（如果需要）
        if (outputSize.isValid() && !image.isNull()) {
            image = image.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        return image.convertToFormat(QImage::Format_RGB32);

    } catch (const winrt::hresult_error& e) {
        qDebug() << "[WGC] captureWindowWithWGC hresult_error:" << QString::fromWCharArray(e.message().c_str());
        return {};
    } catch (...) {
        qDebug() << "[WGC] captureWindowWithWGC unknown exception";
        return {};
    }
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
