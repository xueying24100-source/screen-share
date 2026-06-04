#ifndef SCREENCAPTURER_H
#define SCREENCAPTURER_H

#include <QImage>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QTimer>

struct CaptureFrameMetadata {
    QSize sourceSize;
    QRect sourceGeometry;
    quintptr windowHandle = 0;
    QString backendName;
    qint64 frameIndex = 0;
};

class ScreenCapturer : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapturer(QObject *parent = nullptr);
    ~ScreenCapturer();

    void start(int fps = 15);
    void startScreen(int screenIndex, int fps = 15);
    void startWindow(quintptr windowId, int fps = 15);
    void stop();
    bool isRunning() const { return m_running; }

    void setOutputSize(const QSize &size) { m_outputSize = size; }
    QSize outputSize() const { return m_outputSize; }

    static QImage captureWindowOnce(quintptr windowId, const QSize &outputSize = {});

signals:
    void frameCaptured(const QImage &frame);
    void captureError(const QString &msg);
    void frameMetadataChanged(const CaptureFrameMetadata &meta);

private slots:
    void captureFrame();

private:
    enum class CaptureMode { PrimaryScreen, IndexedScreen, Window };

    void captureScreen();
    void captureWindow();
    void emitFrame(const QImage &frame, const QString &backendName, const QSize &sourceSize,
                   const QRect &sourceGeometry = {});

    QTimer *m_timer = nullptr;
    QSize m_outputSize{1280, 720};
    bool m_running = false;
    CaptureMode m_captureMode = CaptureMode::PrimaryScreen;
    int m_screenIndex = 0;
    quintptr m_windowHandle = 0;
    qint64 m_frameIndex = 0;
    void *m_streamContext = nullptr;
};

#endif // SCREENCAPTURER_H

