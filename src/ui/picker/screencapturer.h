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

    void startWindow(quintptr windowId, int fps = 15);
    void stop();
    bool isRunning() const { return m_running; }

    void setOutputSize(const QSize &size) { m_outputSize = size; }
    QSize outputSize() const { return m_outputSize; }

    static bool imageLooksMostlyBlack(const QImage &imageIn);
    static QImage captureWindowOnce(quintptr windowId, const QSize &outputSize = QSize(160, 90));

signals:
    void frameCaptured(const QImage &frame);
    void captureError(const QString &msg);
    void frameMetadataChanged(const CaptureFrameMetadata &meta);

private slots:
    void captureFrame();

private:
    void captureWindow();

    QTimer *m_timer = nullptr;
    QSize m_outputSize;
    bool m_running = false;
    quintptr m_windowHandle = 0;
    qint64 m_frameIndex = 0;
};

#endif // SCREENCAPTURER_H
