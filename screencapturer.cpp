#include "screencapturer.h"
#include "wgcwindowcapturebackend.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <cstring>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dxgi1_2.h>
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#endif

namespace {
QString windowCaptureUnavailableMessage()
{
    return QStringLiteral("无法捕获该窗口的内容；该窗口可能被其它窗口遮挡、最小化或受 DRM 保护");
}

QImage prepareOutputFrame(const QImage& frame, const QSize& outputSize)
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

ScreenCapturer::ScreenCapturer(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ScreenCapturer::captureFrame);
#ifdef Q_OS_WIN
    m_wgcBackend = std::make_unique<WgcWindowCaptureBackend>();
#endif
}

ScreenCapturer::~ScreenCapturer()
{
    stop();
}

void ScreenCapturer::start(int fps)
{
    if (m_running) return;
    if (!m_preserveModeForStart) {
        m_captureMode = CaptureMode::PrimaryScreen;
        m_screenIndex = 0;
        m_windowHandle = 0;
    }
    m_preserveModeForStart = false;
    m_paused = false;
    m_lastEmittedFrame = QImage{};
    int interval = (fps > 0) ? (1000 / fps) : 33;
    m_timer->start(interval);
    m_running = true;
    qDebug() << "[ScreenCapturer] started, interval =" << interval << "ms";
}

void ScreenCapturer::startScreen(int screenIndex, int fps)
{
    if (m_running) {
        stop();
    }
    m_captureMode = CaptureMode::IndexedScreen;
    m_screenIndex = screenIndex;
    m_windowHandle = 0;
    m_preserveModeForStart = true;
    start(fps);
}

void ScreenCapturer::startWindow(quintptr windowId, int fps)
{
    if (m_running) {
        stop();
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd || !IsWindow(hwnd)) {
        emit captureError(QStringLiteral("Invalid window handle"));
        return;
    }
    if (IsIconic(hwnd)) {
        emit captureError(windowCaptureUnavailableMessage());
        return;
    }
#endif
    m_captureMode = CaptureMode::Window;
    m_windowHandle = windowId;
    m_preserveModeForStart = true;
    start(fps);
}

void ScreenCapturer::stop()
{
    if (!m_running) return;
    m_timer->stop();
    m_running = false;
#ifdef Q_OS_WIN
    if (m_wgcBackend) m_wgcBackend->stop();
#endif
    m_wgcFailed = false;
    m_blackFrameCount = 0;
    m_frameIndex = 0;
    m_lastWgcFrame = QImage{};
    m_lastEmittedFrame = QImage{};
    m_lastWgcFrameTimeMs = 0;
    m_paused = false;
    m_state = CaptureState::Stopped;
    qDebug() << "[ScreenCapturer] stopped";
}

void ScreenCapturer::pause()
{
    m_paused = true;
}

void ScreenCapturer::resume()
{
    m_paused = false;
}

void ScreenCapturer::setWgcOptions(bool cursor, bool border, int minUpdateMs)
{
    m_wgcCursorEnabled = cursor;
    m_wgcBorderRequired = border;
    m_wgcMinUpdateIntervalMs = qMax(0, minUpdateMs);
}

void ScreenCapturer::captureFrame()
{
    if (m_paused) {
        if (!m_lastEmittedFrame.isNull()) {
            emit frameCaptured(m_lastEmittedFrame);
        }
        return;
    }

#ifdef Q_OS_WIN
    if (m_captureMode == CaptureMode::Window) {
        // 窗口关闭检测
        HWND hwnd = reinterpret_cast<HWND>(m_windowHandle);
        if (!IsWindow(hwnd)) {
            emit captureError("Window closed");
            stop();
            return;
        }
        if (IsIconic(hwnd)) {
            emit captureError(windowCaptureUnavailableMessage());
            return;
        }

        // WGC 主路径
        if (!m_wgcFailed && WgcWindowCaptureBackend::isSupported()) {
            if (!m_wgcBackend->isRunning()) {
                m_state = CaptureState::Starting;
                m_wgcBackend->setCursorCaptureEnabled(m_wgcCursorEnabled);
                m_wgcBackend->setBorderRequired(m_wgcBorderRequired);
                m_wgcBackend->setMinUpdateInterval(m_wgcMinUpdateIntervalMs);
                if (!m_wgcBackend->start(hwnd)) {
                    emit captureError(QStringLiteral("WGC 启动失败，已降级到 GDI 捕获"));
                    qDebug() << "[ScreenCapturer] WGC start failed, fallback to GDI";
                    m_wgcFailed = true;
                } else {
                    m_state = CaptureState::Running;
                }
            }

            if (!m_wgcFailed) {
                QImage frame = m_wgcBackend->tryGetFrame();
                if (!frame.isNull()) {
                    m_lastWgcFrame = frame;
                    m_lastWgcFrameTimeMs = QDateTime::currentMSecsSinceEpoch();
                    // 黑帧检测
                    QPixmap tmp = QPixmap::fromImage(frame);
                    if (pixmapLooksMostlyBlack(tmp)) {
                        ++m_blackFrameCount;
                        if (m_blackFrameCount >= m_blackFrameThreshold) {
                            emit captureError(QStringLiteral("WGC 连续黑帧，已降级到 GDI 捕获"));
                            qDebug() << "[ScreenCapturer] WGC consecutive black frames, fallback to GDI";
                            m_wgcFailed = true;
                            m_wgcBackend->stop();
                            m_blackFrameCount = 0;
                            m_state = CaptureState::Recovering;
                        }
                    } else {
                        m_blackFrameCount = 0;
                    }

                    if (!m_wgcFailed) {
                        ++m_frameIndex;
                        m_lastEmittedFrame = frame;
                        emit frameCaptured(frame);

                        CaptureFrameMetadata meta;
                        meta.sourceSize    = m_wgcBackend->lastFrameSize();
                        meta.windowHandle  = m_windowHandle;
                        meta.backendName   = QStringLiteral("WGC");
                        meta.frameIndex    = m_frameIndex;
                        emit frameMetadataChanged(meta);
                        return;
                    }
                } else {
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if (!m_lastWgcFrame.isNull() && (now - m_lastWgcFrameTimeMs) <= 200) {
                        m_lastEmittedFrame = m_lastWgcFrame;
                        emit frameCaptured(m_lastWgcFrame);
                    }
                    return;
                }
            }
        }

        // WGC 不可用或已失败，走 GDI fallback
        captureWithGdiWindow();
        return;
    }

    // 屏幕采集路径（保持不变）
    if (m_captureMode != CaptureMode::Window && m_useDXGI) {
        if (captureWithDXGI()) return;
        // DXGI 失败，自动降级
        m_useDXGI = false;
        qDebug() << "[ScreenCapturer] DXGI unavailable, fallback to grabWindow";
    }
#endif
    captureWithGrabWindow();
}

bool ScreenCapturer::captureWithDXGI()
{
#ifdef Q_OS_WIN
    HRESULT hr = S_OK;

    int outputIndex = 0;
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (m_captureMode == CaptureMode::IndexedScreen) {
        outputIndex = m_screenIndex;
    } else {
        QScreen* primary = QGuiApplication::primaryScreen();
        const int primaryIndex = screens.indexOf(primary);
        outputIndex = (primaryIndex >= 0) ? primaryIndex : 0;
    }

    if (outputIndex < 0 || outputIndex >= screens.size()) {
        return false;
    }

    if (!m_d3dDevice || !m_d3dContext || !m_duplication || m_dxgiOutputIndex != outputIndex) {
        m_duplication.Reset();
        m_stagingTexture.Reset();
        m_captureWidth = 0;
        m_captureHeight = 0;

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };

        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels,
            static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
            D3D11_SDK_VERSION,
            m_d3dDevice.ReleaseAndGetAddressOf(),
            &featureLevel,
            m_d3dContext.ReleaseAndGetAddressOf()
        );
        if (FAILED(hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        hr = m_d3dDevice.As(&dxgiDevice);
        if (FAILED(hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        hr = adapter->EnumOutputs(static_cast<UINT>(outputIndex), output.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
        hr = output.As(&output1);
        if (FAILED(hr)) {
            return false;
        }

        hr = output1->DuplicateOutput(m_d3dDevice.Get(), m_duplication.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
        m_dxgiOutputIndex = outputIndex;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
    hr = m_duplication->AcquireNextFrame(0, &frameInfo, desktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return false;
    }
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            m_duplication.Reset();
            m_stagingTexture.Reset();
            m_captureWidth = 0;
            m_captureHeight = 0;
        }
        return false;
    }

    bool frameAcquired = true;
    auto releaseFrame = [&]() {
        if (frameAcquired && m_duplication) {
            m_duplication->ReleaseFrame();
            frameAcquired = false;
        }
    };

    Microsoft::WRL::ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) {
        releaseFrame();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desktopTexture->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0) {
        releaseFrame();
        return false;
    }

    if (!m_stagingTexture || m_captureWidth != desc.Width || m_captureHeight != desc.Height) {
        D3D11_TEXTURE2D_DESC stagingDesc = desc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;
        stagingDesc.ArraySize = 1;
        stagingDesc.MipLevels = 1;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.SampleDesc.Quality = 0;

        hr = m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, m_stagingTexture.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            releaseFrame();
            return false;
        }
        m_captureWidth = desc.Width;
        m_captureHeight = desc.Height;
    }

    m_d3dContext->CopyResource(m_stagingTexture.Get(), desktopTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = m_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        releaseFrame();
        return false;
    }

    QImage frame(static_cast<int>(desc.Width), static_cast<int>(desc.Height), QImage::Format_RGB32);
    if (frame.isNull()) {
        m_d3dContext->Unmap(m_stagingTexture.Get(), 0);
        releaseFrame();
        return false;
    }

    const int srcBytesPerLine = static_cast<int>(mapped.RowPitch);
    const int dstBytesPerLine = frame.bytesPerLine();
    const int copyBytes = std::min(srcBytesPerLine, dstBytesPerLine);
    const auto* src = static_cast<const unsigned char*>(mapped.pData);
    for (int y = 0; y < frame.height(); ++y) {
        std::memcpy(frame.scanLine(y), src + (static_cast<size_t>(y) * mapped.RowPitch), static_cast<size_t>(copyBytes));
    }

    m_d3dContext->Unmap(m_stagingTexture.Get(), 0);
    releaseFrame();

    QImage output = prepareOutputFrame(frame, m_outputSize);
    ++m_frameIndex;
    m_lastEmittedFrame = output;
    emit frameCaptured(output);

    CaptureFrameMetadata meta;
    meta.sourceSize = QSize(static_cast<int>(m_captureWidth), static_cast<int>(m_captureHeight));
    if (m_captureMode == CaptureMode::IndexedScreen && m_screenIndex >= 0 && m_screenIndex < screens.size()) {
        meta.sourceGeometry = screens.at(m_screenIndex)->geometry();
    } else if (QScreen* primary = QGuiApplication::primaryScreen()) {
        meta.sourceGeometry = primary->geometry();
    }
    meta.backendName = QStringLiteral("DXGI");
    meta.frameIndex = m_frameIndex;
    emit frameMetadataChanged(meta);
    return true;
#else
    return false;
#endif
}

void ScreenCapturer::captureWithGdiWindow()
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_windowHandle);
    if (!hwnd || !IsWindow(hwnd)) {
        emit captureError(QStringLiteral("Invalid window handle"));
        return;
    }
    if (IsIconic(hwnd)) {
        emit captureError(windowCaptureUnavailableMessage());
        return;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        emit captureError(QStringLiteral("GetWindowRect failed"));
        return;
    }
    const int width  = rect.right  - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        emit captureError(QStringLiteral("Window has invalid size"));
        return;
    }

    auto captureFromDC = [&](bool usePrintWindow) -> QPixmap {
        HDC screenDc = GetDC(nullptr);
        if (!screenDc) return {};
        HDC memDc = CreateCompatibleDC(screenDc);
        if (!memDc) { ReleaseDC(nullptr, screenDc); return {}; }
        HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
        if (!bitmap) { DeleteDC(memDc); ReleaseDC(nullptr, screenDc); return {}; }

        HGDIOBJ oldObj = SelectObject(memDc, bitmap);
        bool ok = false;
        if (usePrintWindow) {
            ok = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
        } else {
            HDC windowDc = GetWindowDC(hwnd);
            if (windowDc) {
                ok = BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY | CAPTUREBLT);
                ReleaseDC(hwnd, windowDc);
            }
        }

        QPixmap result;
        if (ok) {
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = width;
            bmi.bmiHeader.biHeight      = -height;
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            QImage image(width, height, QImage::Format_ARGB32);
            if (!image.isNull()) {
                if (GetDIBits(memDc, bitmap, 0,
                              static_cast<UINT>(height),
                              image.bits(), &bmi, DIB_RGB_COLORS) > 0) {
                    result = QPixmap::fromImage(image);
                }
            }
        }

        SelectObject(memDc, oldObj);
        DeleteObject(bitmap);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return result;
    };

    QString backendUsed;
    QPixmap pixmap = captureFromDC(true);
    if (!pixmap.isNull() && !pixmapLooksMostlyBlack(pixmap)) {
        backendUsed = QStringLiteral("PrintWindow");
    } else {
        pixmap = captureFromDC(false);
        if (!pixmap.isNull() && !pixmapLooksMostlyBlack(pixmap)) {
            backendUsed = QStringLiteral("BitBlt");
        }
    }

    if (pixmap.isNull() || pixmapLooksMostlyBlack(pixmap)) {
        emit captureError(windowCaptureUnavailableMessage());
        return;
    }

    // 原始分辨率输出，不强制缩放
    QImage frame = pixmap.toImage().convertToFormat(QImage::Format_RGB32);
    if (frame.isNull()) {
        emit captureError(QStringLiteral("GDI frame conversion failed"));
        return;
    }

    ++m_frameIndex;
    m_lastEmittedFrame = frame;
    emit frameCaptured(frame);

    CaptureFrameMetadata meta;
    meta.sourceSize     = frame.size();
    meta.sourceGeometry = QRect(rect.left, rect.top, width, height);
    meta.windowHandle   = m_windowHandle;
    meta.backendName    = backendUsed;
    meta.frameIndex     = m_frameIndex;
    emit frameMetadataChanged(meta);
#endif
}

void ScreenCapturer::captureWithGrabWindow()
{
    QPixmap pixmap;
    QString backendUsed = QStringLiteral("GrabWindow");

    if (m_captureMode == CaptureMode::PrimaryScreen) {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) {
            emit captureError("No primary screen found");
            return;
        }
        pixmap = screen->grabWindow(0);
    } else if (m_captureMode == CaptureMode::IndexedScreen) {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (m_screenIndex < 0 || m_screenIndex >= screens.size()) {
            emit captureError(QStringLiteral("Screen index out of range: %1").arg(m_screenIndex));
            return;
        }
        QScreen* screen = screens.at(m_screenIndex);
        if (!screen) {
            emit captureError(QStringLiteral("Screen not available"));
            return;
        }
        pixmap = screen->grabWindow(0);
    } else {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_windowHandle);
        if (!hwnd || !IsWindow(hwnd)) {
            emit captureError(QStringLiteral("Invalid window handle"));
            return;
        }
        if (IsIconic(hwnd)) {
            emit captureError(windowCaptureUnavailableMessage());
            return;
        }

        RECT rect{};
        if (!GetWindowRect(hwnd, &rect)) {
            emit captureError(QStringLiteral("GetWindowRect failed"));
            return;
        }
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) {
            emit captureError(QStringLiteral("Window has invalid size"));
            return;
        }

        auto captureFromDC = [&](bool usePrintWindow) -> QPixmap {
            HDC screenDc = GetDC(nullptr);
            if (!screenDc) {
                return {};
            }
            HDC memDc = CreateCompatibleDC(screenDc);
            if (!memDc) {
                ReleaseDC(nullptr, screenDc);
                return {};
            }
            HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
            if (!bitmap) {
                DeleteDC(memDc);
                ReleaseDC(nullptr, screenDc);
                return {};
            }

            HGDIOBJ oldObj = SelectObject(memDc, bitmap);
            bool ok = false;
            if (usePrintWindow) {
                ok = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
            } else {
                HDC windowDc = GetWindowDC(hwnd);
                if (windowDc) {
                    ok = BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY | CAPTUREBLT);
                    ReleaseDC(hwnd, windowDc);
                }
            }

            QPixmap result;
            if (ok) {
                BITMAPINFO bmi{};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = width;
                bmi.bmiHeader.biHeight = -height;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                QImage image(width, height, QImage::Format_ARGB32);
                if (!image.isNull()) {
                    if (GetDIBits(memDc,
                                  bitmap,
                                  0,
                                  static_cast<UINT>(height),
                                  image.bits(),
                                  &bmi,
                                  DIB_RGB_COLORS) > 0) {
                        result = QPixmap::fromImage(image);
                    }
                }
            }

            SelectObject(memDc, oldObj);
            DeleteObject(bitmap);
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
            return result;
        };

        pixmap = captureFromDC(true);
        if (!pixmap.isNull() && !pixmapLooksMostlyBlack(pixmap)) {
            backendUsed = QStringLiteral("PrintWindow");
        } else {
            pixmap = captureFromDC(false);
            if (!pixmap.isNull() && !pixmapLooksMostlyBlack(pixmap)) {
                backendUsed = QStringLiteral("BitBlt");
            }
        }
        if (pixmap.isNull() || pixmapLooksMostlyBlack(pixmap)) {
            emit captureError(windowCaptureUnavailableMessage());
            return;
        }
#else
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) {
            emit captureError("No primary screen found");
            return;
        }
        pixmap = screen->grabWindow(static_cast<WId>(m_windowHandle));
#endif
    }

    if (pixmap.isNull()) {
        emit captureError("grabWindow() returned null pixmap");
        return;
    }

    QImage frame = prepareOutputFrame(pixmap.toImage(), m_outputSize);

    ++m_frameIndex;
    m_lastEmittedFrame = frame;
    emit frameCaptured(frame);

    CaptureFrameMetadata meta;
    meta.sourceSize = pixmap.size();
    if (m_captureMode == CaptureMode::PrimaryScreen) {
        if (QScreen* primary = QGuiApplication::primaryScreen()) {
            meta.sourceGeometry = primary->geometry();
        }
        meta.backendName = QStringLiteral("GrabWindow");
    } else if (m_captureMode == CaptureMode::IndexedScreen) {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (m_screenIndex >= 0 && m_screenIndex < screens.size() && screens.at(m_screenIndex)) {
            meta.sourceGeometry = screens.at(m_screenIndex)->geometry();
        }
        meta.backendName = QStringLiteral("GrabWindow");
    } else {
#ifdef Q_OS_WIN
        RECT rect{};
        const HWND hwnd = reinterpret_cast<HWND>(m_windowHandle);
        if (hwnd && GetWindowRect(hwnd, &rect)) {
            meta.sourceGeometry = QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        }
        meta.windowHandle = m_windowHandle;
        meta.backendName = backendUsed;
#else
        meta.backendName = QStringLiteral("GrabWindow");
#endif
    }
    meta.frameIndex = m_frameIndex;
    emit frameMetadataChanged(meta);
}

bool ScreenCapturer::pixmapLooksMostlyBlack(const QPixmap& pixmap) const
{
    if (pixmap.isNull()) {
        return true;
    }

    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGB32);
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
