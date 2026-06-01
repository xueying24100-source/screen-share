#include "cameramanager.h"

#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QVideoFrame>
#include <QVideoSink>
#include <QDebug>
#include <QDateTime>
#include <QDateTime>

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
{
}

CameraManager::~CameraManager()
{
    stop();
}

QStringList CameraManager::availableCameraNames() const
{
    QStringList names;
    const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    for (const QCameraDevice &device : devices) {
        names << device.description();
    }
    return names;
}

bool CameraManager::startDefaultCamera()
{
    return startCameraByIndex(0);
}

bool CameraManager::startCameraByIndex(int index)
{
    const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        emit cameraError(QStringLiteral("未检测到可用摄像头"));
        return false;
    }
    if (index < 0 || index >= devices.size()) {
        emit cameraError(QStringLiteral("摄像头索引无效"));
        return false;
    }

    stop();

    const QCameraDevice device = devices.at(index);
    m_session = new QMediaCaptureSession(this);
    m_videoSink = new QVideoSink(this);
    m_camera = new QCamera(device, this);

    m_session->setCamera(m_camera);
    m_session->setVideoSink(m_videoSink);

    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &CameraManager::onVideoFrameChanged);
    connect(m_camera, &QCamera::errorOccurred, this,
            [this](QCamera::Error error, const QString &errorString) {
                if (error == QCamera::NoError) {
                    return;
                }
                const QString message = errorString.isEmpty()
                                            ? QStringLiteral("摄像头打开失败")
                                            : errorString;
                emit cameraError(message);
            });

    m_lastFrameMs = 0;
    m_lastFrameMs = 0;
    m_camera->start();
    m_running = true;
    emit cameraStarted(device.description());
    return true;
}

void CameraManager::stop()
{
    if (m_camera) {
        m_camera->stop();
    }

    delete m_camera;
    delete m_videoSink;
    delete m_session;

    m_camera = nullptr;
    m_videoSink = nullptr;
    m_session = nullptr;

    if (m_running) {
        m_running = false;
        emit cameraStopped();
    }
}

void CameraManager::setTargetFps(int fps)
{
    m_targetFps = qBound(1, fps, 30);
}

void CameraManager::setMaxFrameSize(const QSize &size)
{
    m_maxFrameSize = size;
}

void CameraManager::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!m_running || !frame.isValid()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 intervalMs = 1000 / qMax(1, m_targetFps);
    if (m_lastFrameMs != 0 && nowMs - m_lastFrameMs < intervalMs) {
        return;
    }
    m_lastFrameMs = nowMs;

    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }

    // 统一转换成常见格式，避免 JPEG 编码或 QLabel 显示时遇到平台相关格式问题。
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_RGB32);
    }

    // 摄像头只显示在小窗，不需要高清；先降分辨率可以明显减少双开测试卡顿。
    if (m_maxFrameSize.isValid() &&
        (image.width() > m_maxFrameSize.width() || image.height() > m_maxFrameSize.height())) {
        image = image.scaled(m_maxFrameSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    emit frameReady(image);
}
