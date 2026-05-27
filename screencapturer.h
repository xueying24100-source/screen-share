#pragma once
#define NOMINMAX
#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QTimer>

#ifdef Q_OS_WIN
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#endif

class ScreenCapturer : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapturer(QObject* parent = nullptr);
    ~ScreenCapturer();

    void start(int fps = 30);
    void startScreen(int screenIndex, int fps = 30);
    void startWindow(quintptr windowId, int fps = 30);
    void stop();
    bool isRunning() const { return m_running; }

    void setOutputSize(const QSize& size) { m_outputSize = size; }
    QSize outputSize() const { return m_outputSize; }

signals:
    void frameCaptured(const QImage& frame);
    void captureError(const QString& msg);

private slots:
    void captureFrame();

private:
    enum class CaptureMode { PrimaryScreen, IndexedScreen, Window };

    bool captureWithDXGI();
    void captureWithGrabWindow();
    bool pixmapLooksMostlyBlack(const QPixmap& pixmap) const;

    QTimer* m_timer;
    QSize   m_outputSize{1280, 720};
    bool    m_useDXGI{true};
    bool    m_running{false};
    CaptureMode m_captureMode{CaptureMode::PrimaryScreen};
    int         m_screenIndex{0};
    quintptr    m_windowHandle{0};
    bool        m_preserveModeForStart{false};

#ifdef Q_OS_WIN
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_duplication;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;
    UINT m_captureWidth{0};
    UINT m_captureHeight{0};
    int  m_dxgiOutputIndex{-1};
#endif
};
