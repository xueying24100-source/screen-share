#include "media/capture/screen/screencapturer.h"
#include "platform/windows/wgc/wgcwindowcapturebackend.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QThread>
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
    m_statsWindowStartMs = QDateTime::currentMSecsSinceEpoch();
    m_statsWindowFrames = 0;
    m_inFlightFrames.store(0, std::memory_order_release);
    int interval = (fps > 0) ? (1000 / fps) : 33;
    m_timer->start(interval);
    m_running = true;
    qDebug() << "[ScreenCapturer] thread=" << QThread::currentThread()
             << "started, interval =" << interval << "ms";
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
    m_wgcFallbackLoggedForSession = false;
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
    m_statsWindowStartMs = 0;
    m_statsWindowFrames = 0;
    m_wgcFallbackLoggedForSession = false;
    m_inFlightFrames.store(0, std::memory_order_release);
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
            emitFrameWithStats(m_lastEmittedFrame, QStringLiteral("PausedReplay"), m_lastEmittedFrame.size());
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
                    if (!m_wgcFallbackLoggedForSession) {
                        qDebug() << "[ScreenCapturer] WGC unavailable for this window, using GrabWindow; expect low fps on large windows";
                        m_wgcFallbackLoggedForSession = true;
                    }
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
                    if (imageLooksMostlyBlack(frame)) {
                        ++m_blackFrameCount;
                        if (m_blackFrameCount >= m_blackFrameThreshold) {
                            emit captureError(QStringLiteral("WGC 连续黑帧，已降级到 GDI 捕获"));
                            qDebug() << "[ScreenCapturer] WGC consecutive black frames, fallback to GDI";
                            if (!m_wgcFallbackLoggedForSession) {
                                qDebug() << "[ScreenCapturer] WGC unavailable for this window, using GrabWindow; expect low fps on large windows";
                                m_wgcFallbackLoggedForSession = true;
                            }
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
                        QImage output = frame;
                        if (m_outputSize.isValid()
                            && (frame.width() > m_outputSize.width() || frame.height() > m_outputSize.height())) {
                            output = frame.scaled(m_outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        }
                        emitFrameWithStats(output,
                                           QStringLiteral("WGC"),
                                           m_wgcBackend->lastFrameSize());

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
                        emitFrameWithStats(m_lastWgcFrame, QStringLiteral("WGC"), m_lastWgcFrame.size());
                    }
                    return;
                }
            }
        }

        // WGC 不可用或已失败，走 GDI fallback
        if (!m_wgcFallbackLoggedForSession) {
            qDebug() << "[ScreenCapturer] WGC unavailable for this window, using GrabWindow; expect low fps on large windows";
            m_wgcFallbackLoggedForSession = true;
        }
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
    emitFrameWithStats(output,
                       QStringLiteral("DXGI"),
                       QSize(static_cast<int>(m_captureWidth), static_cast<int>(m_captureHeight)));

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

    auto captureFromDC = [&](bool usePrintWindow) -> QImage {
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

        QImage result;
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
                    result = image;
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
    QImage capturedImage = captureFromDC(true);
    if (!capturedImage.isNull() && !imageLooksMostlyBlack(capturedImage)) {
        backendUsed = QStringLiteral("PrintWindow");
    } else {
        capturedImage = captureFromDC(false);
        if (!capturedImage.isNull() && !imageLooksMostlyBlack(capturedImage)) {
            backendUsed = QStringLiteral("BitBlt");
        }
    }

    if (capturedImage.isNull() || imageLooksMostlyBlack(capturedImage)) {
        emit captureError(windowCaptureUnavailableMessage());
        return;
    }

    QImage frame = capturedImage.convertToFormat(QImage::Format_RGB32);
    if (frame.isNull()) {
        emit captureError(QStringLiteral("GDI frame conversion failed"));
        return;
    }
    if (m_outputSize.isValid()
        && (frame.width() > m_outputSize.width() || frame.height() > m_outputSize.height())) {
        frame = frame.scaled(m_outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    ++m_frameIndex;
    emitFrameWithStats(frame, backendUsed, frame.size());

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
    QString backendUsed = QStringLiteral("GrabWindow");

    if (m_captureMode == CaptureMode::PrimaryScreen) {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) {
            emit captureError("No primary screen found");
            return;
        }
        QPixmap pixmap = screen->grabWindow(0);
        if (pixmap.isNull()) {
            emit captureError("grabWindow() returned null pixmap");
            return;
        }

        QImage frame = prepareOutputFrame(pixmap.toImage(), m_outputSize);

        ++m_frameIndex;
        emitFrameWithStats(frame, backendUsed, pixmap.size());

        CaptureFrameMetadata meta;
        meta.sourceSize = pixmap.size();
        meta.sourceGeometry = screen->geometry();
        meta.backendName = QStringLiteral("GrabWindow");
        meta.frameIndex = m_frameIndex;
        emit frameMetadataChanged(meta);
        return;
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
        QPixmap pixmap = screen->grabWindow(0);
        if (pixmap.isNull()) {
            emit captureError("grabWindow() returned null pixmap");
            return;
        }

        QImage frame = prepareOutputFrame(pixmap.toImage(), m_outputSize);

        ++m_frameIndex;
        emitFrameWithStats(frame, backendUsed, pixmap.size());

        CaptureFrameMetadata meta;
        meta.sourceSize = pixmap.size();
        meta.sourceGeometry = screen->geometry();
        meta.backendName = QStringLiteral("GrabWindow");
        meta.frameIndex = m_frameIndex;
        emit frameMetadataChanged(meta);
        return;
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

        auto captureFromDC = [&](bool usePrintWindow) -> QImage {
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

            QImage result;
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
                        result = image;
                    }
                }
            }

            SelectObject(memDc, oldObj);
            DeleteObject(bitmap);
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
            return result;
        };

        QImage windowImage = captureFromDC(true);
        if (!windowImage.isNull() && !imageLooksMostlyBlack(windowImage)) {
            backendUsed = QStringLiteral("PrintWindow");
        } else {
            windowImage = captureFromDC(false);
            if (!windowImage.isNull() && !imageLooksMostlyBlack(windowImage)) {
                backendUsed = QStringLiteral("BitBlt");
            }
        }
        if (windowImage.isNull() || imageLooksMostlyBlack(windowImage)) {
            emit captureError(windowCaptureUnavailableMessage());
            return;
        }

        QImage frame = prepareOutputFrame(windowImage, m_outputSize);

        ++m_frameIndex;
        emitFrameWithStats(frame, backendUsed, windowImage.size());

        CaptureFrameMetadata meta;
        meta.sourceSize = windowImage.size();
        RECT metaRect{};
        if (GetWindowRect(hwnd, &metaRect)) {
            meta.sourceGeometry = QRect(metaRect.left,
                                        metaRect.top,
                                        metaRect.right - metaRect.left,
                                        metaRect.bottom - metaRect.top);
        }
        meta.windowHandle = m_windowHandle;
        meta.backendName = backendUsed;
        meta.frameIndex = m_frameIndex;
        emit frameMetadataChanged(meta);
        return;
#else
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) {
            emit captureError("No primary screen found");
            return;
        }
        QImage windowImage = screen->grabWindow(static_cast<WId>(m_windowHandle)).toImage();
        if (windowImage.isNull()) {
            emit captureError("grabWindow() returned null image");
            return;
        }

        QImage frame = prepareOutputFrame(windowImage, m_outputSize);

        ++m_frameIndex;
        emitFrameWithStats(frame, backendUsed, windowImage.size());

        CaptureFrameMetadata meta;
        meta.sourceSize = windowImage.size();
        meta.backendName = QStringLiteral("GrabWindow");
        meta.frameIndex = m_frameIndex;
        emit frameMetadataChanged(meta);
        return;
#endif
    }
}

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

void ScreenCapturer::emitFrameWithStats(const QImage& frame, const QString& backendName, const QSize& sourceSize)
{
    m_lastEmittedFrame = frame;
    if (m_inFlightFrames.exchange(1, std::memory_order_acq_rel) != 0) {
        return;
    }
    emit frameCaptured(frame);

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_statsWindowStartMs <= 0) {
        m_statsWindowStartMs = nowMs;
        m_statsWindowFrames = 0;
    }
    ++m_statsWindowFrames;
    const qint64 elapsedMs = nowMs - m_statsWindowStartMs;
    if (elapsedMs >= 1000) {
        const double fps = (elapsedMs > 0)
                               ? (static_cast<double>(m_statsWindowFrames) * 1000.0 / static_cast<double>(elapsedMs))
                               : 0.0;
        qDebug() << "[ScreenCapturer] stats fps=" << fps
                 << "backend=" << backendName
                 << "source=" << sourceSize;
        m_statsWindowStartMs = nowMs;
        m_statsWindowFrames = 0;
    }
}

void ScreenCapturer::releaseFrameSlot()
{
    m_inFlightFrames.store(0, std::memory_order_release);
}
