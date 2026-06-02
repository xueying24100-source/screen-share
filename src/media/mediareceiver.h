#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>
#include <QThread>
#include <QHash>

class VideoDecodeWorker;

class MediaReceiver : public QObject
{
    Q_OBJECT
public:
    explicit MediaReceiver(QObject *parent = nullptr);
    ~MediaReceiver() override;

public slots:
    void onPacketReceived(const QByteArray &packet);

private slots:
    void onVideoFrameDecoded(quint8 sourceKindRaw, const QImage &image);
    void onVideoDecodeFailed(quint8 sourceKindRaw, const QString &message);

signals:
    void mainVideoFrameReceived(const QImage &image);
    void cameraFrameReceived(const QImage &image);
    void cameraStoppedReceived();
    void audioFrameReceived(const QByteArray &pcm);
    void receiverMessage(const QString &message);

private:
    void submitVideoForDecode(quint8 sourceKindRaw, const QByteArray &payload);
    void submitPendingVideoIfAny(quint8 sourceKindRaw);

    QThread m_videoDecodeThread;
    VideoDecodeWorker *m_videoDecodeWorker = nullptr;
    QHash<quint8, bool> m_videoDecodeInFlight;
    QHash<quint8, QByteArray> m_pendingVideoPayload;
    bool m_ignoreNextCameraDecodedFrame = false;
};
