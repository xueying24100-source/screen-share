#pragma once

#include <QObject>
#include <QImage>
#include <QStringList>
#include <QScopedPointer>
#include <QtGlobal>
#include <QSize>

class QCamera;
class QMediaCaptureSession;
class QVideoSink;
class QVideoFrame;

class CameraManager : public QObject
{
    Q_OBJECT
public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager() override;

    QStringList availableCameraNames() const;
    bool startDefaultCamera();
    bool startCameraByIndex(int index);
    void stop();
    bool isRunning() const { return m_running; }

    void setTargetFps(int fps);
    void setMaxFrameSize(const QSize &size);

signals:
    void frameReady(const QImage &image);
    void cameraStarted(const QString &deviceName);
    void cameraStopped();
    void cameraError(const QString &message);

private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);

private:
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_session = nullptr;
    QVideoSink *m_videoSink = nullptr;
    bool m_running = false;
    int m_targetFps = 12;
    QSize m_maxFrameSize = QSize(640, 360);
    qint64 m_lastFrameMs = 0;
};
