#include <QtGlobal>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WINRT_LEAN_AND_MEAN
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#endif

#include "wgcwindowcapturebackend.h"
#ifdef Q_OS_WIN

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <QDebug>
#include <chrono>

struct WgcWindowCaptureBackend::Impl {
    Microsoft::WRL::ComPtr<ID3D11Device>        d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     stagingTexture;
    UINT stagingWidth{0};
    UINT stagingHeight{0};

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem      captureItem{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession   session{nullptr};
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{nullptr};

    bool  running{false};
    QSize lastSize;
    bool  cursorCaptureEnabled{true};
    bool  borderRequired{true};
    int   minUpdateIntervalMs{0};

    void reset() {
        if (session)    { try { session.Close();   } catch (...) {} session    = nullptr; }
        if (framePool)  { try { framePool.Close();  } catch (...) {} framePool  = nullptr; }
        captureItem  = nullptr;
        winrtDevice  = nullptr;
        stagingTexture.Reset();
        stagingWidth  = 0;
        stagingHeight = 0;
        running       = false;
    }
};

WgcWindowCaptureBackend::WgcWindowCaptureBackend()
    : m_impl(std::make_unique<Impl>())
{}

WgcWindowCaptureBackend::~WgcWindowCaptureBackend()
{
    stop();
}

bool WgcWindowCaptureBackend::isSupported()
{
    static int cached = -1;
    if (cached < 0) {
        try {
            cached = winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported() ? 1 : 0;
        } catch (...) {
            cached = 0;
        }
    }
    return cached == 1;
}

bool WgcWindowCaptureBackend::start(HWND hwnd)
{
    stop();

    try {
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
            m_impl->d3dDevice.ReleaseAndGetAddressOf(),
            &featureLevel,
            m_impl->d3dContext.ReleaseAndGetAddressOf()
        );
        if (FAILED(hr)) {
            qDebug() << "[WGC] D3D11CreateDevice failed, hr =" << hr;
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        hr = m_impl->d3dDevice.As(&dxgiDevice);
        if (FAILED(hr)) {
            qDebug() << "[WGC] QI IDXGIDevice failed";
            return false;
        }

        winrt::com_ptr<::IInspectable> inspectable;
        hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
        if (FAILED(hr)) {
            qDebug() << "[WGC] CreateDirect3D11DeviceFromDXGIDevice failed, hr =" << hr;
            return false;
        }
        m_impl->winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

        auto factory = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();

        hr = factory->CreateForWindow(
            hwnd,
            winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
            winrt::put_abi(m_impl->captureItem));
        if (FAILED(hr)) {
            qDebug() << "[WGC] CreateForWindow failed, hr =" << hr;
            return false;
        }

        auto size = m_impl->captureItem.Size();

        m_impl->framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
            m_impl->winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            3,
            size);

        m_impl->session = m_impl->framePool.CreateCaptureSession(m_impl->captureItem);

        try {
            m_impl->session.IsCursorCaptureEnabled(m_impl->cursorCaptureEnabled);
        } catch (...) {}

        try {
            m_impl->session.IsBorderRequired(m_impl->borderRequired);
        } catch (...) {}

        m_impl->session.StartCapture();

        m_impl->running = true;
        qDebug() << "[WGC] started, initial size:" << size.Width << "x" << size.Height;
        return true;

    } catch (const winrt::hresult_error& e) {
        qDebug() << "[WGC] start() hresult_error:" << QString::fromWCharArray(e.message().c_str());
        m_impl->reset();
        return false;
    } catch (...) {
        qDebug() << "[WGC] start() unknown exception";
        m_impl->reset();
        return false;
    }
}

void WgcWindowCaptureBackend::stop()
{
    m_impl->reset();
}

bool WgcWindowCaptureBackend::isRunning() const
{
    return m_impl->running;
}

void WgcWindowCaptureBackend::setCursorCaptureEnabled(bool enabled)
{
    m_impl->cursorCaptureEnabled = enabled;
}

void WgcWindowCaptureBackend::setBorderRequired(bool required)
{
    m_impl->borderRequired = required;
}

void WgcWindowCaptureBackend::setMinUpdateInterval(int ms)
{
    m_impl->minUpdateIntervalMs = qMax(0, ms);
}

QImage WgcWindowCaptureBackend::tryGetFrame()
{
    if (!m_impl->running || !m_impl->framePool) {
        return {};
    }

    try {
        auto frame = m_impl->framePool.TryGetNextFrame();
        if (!frame) {
            return {};
        }
        while (auto newerFrame = m_impl->framePool.TryGetNextFrame()) {
            try { frame.Close(); } catch (...) {}
            frame = std::move(newerFrame);
        }
        struct FrameGuard {
            decltype(frame)& frameRef;
            ~FrameGuard() {
                try { frameRef.Close(); } catch (...) {}
            }
        } frameGuard{frame};

        auto surface = frame.Surface();

        auto dxgiAccess = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        Microsoft::WRL::ComPtr<ID3D11Texture2D> frameTex;
        HRESULT hr = dxgiAccess->GetInterface(IID_PPV_ARGS(frameTex.GetAddressOf()));
        if (FAILED(hr)) {
            return {};
        }

        D3D11_TEXTURE2D_DESC desc{};
        frameTex->GetDesc(&desc);
        if (desc.Width == 0 || desc.Height == 0) {
            return {};
        }

        if (!m_impl->stagingTexture
            || m_impl->stagingWidth  != desc.Width
            || m_impl->stagingHeight != desc.Height)
        {
            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.Usage          = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags      = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDesc.MiscFlags      = 0;
            stagingDesc.ArraySize      = 1;
            stagingDesc.MipLevels      = 1;
            stagingDesc.SampleDesc.Count   = 1;
            stagingDesc.SampleDesc.Quality = 0;

            hr = m_impl->d3dDevice->CreateTexture2D(
                &stagingDesc, nullptr,
                m_impl->stagingTexture.ReleaseAndGetAddressOf());
            if (FAILED(hr)) {
                return {};
            }
            m_impl->stagingWidth  = desc.Width;
            m_impl->stagingHeight = desc.Height;
        }

        auto itemSize = m_impl->captureItem.Size();
        if (itemSize.Width  != static_cast<int32_t>(desc.Width)
         || itemSize.Height != static_cast<int32_t>(desc.Height))
        {
            m_impl->framePool.Recreate(
                m_impl->winrtDevice,
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                3,
                itemSize);
        }

        m_impl->d3dContext->CopyResource(m_impl->stagingTexture.Get(), frameTex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = m_impl->d3dContext->Map(m_impl->stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            return {};
        }

        QImage image(static_cast<int>(desc.Width),
                     static_cast<int>(desc.Height),
                     QImage::Format_RGB32);
        if (!image.isNull()) {
            const int dstBytesPerLine = image.bytesPerLine();
            const int copyBytes = std::min(static_cast<int>(mapped.RowPitch), dstBytesPerLine);
            const auto* src = static_cast<const unsigned char*>(mapped.pData);
            for (int y = 0; y < image.height(); ++y) {
                std::memcpy(image.scanLine(y),
                            src + static_cast<size_t>(y) * mapped.RowPitch,
                            static_cast<size_t>(copyBytes));
            }
        }

        m_impl->d3dContext->Unmap(m_impl->stagingTexture.Get(), 0);

        m_impl->lastSize = QSize(static_cast<int>(desc.Width),
                                  static_cast<int>(desc.Height));

        return image;

    } catch (const winrt::hresult_error& e) {
        qDebug() << "[WGC] tryGetFrame() hresult_error:" << QString::fromWCharArray(e.message().c_str());
        return {};
    } catch (...) {
        qDebug() << "[WGC] tryGetFrame() unknown exception";
        return {};
    }
}

QSize WgcWindowCaptureBackend::lastFrameSize() const
{
    return m_impl->lastSize;
}

#endif
