#include "mediareceiver.h"
#include "sender.h"
#include "videodecodeworker.h"

#include <QDataStream>
#include <QDebug>
#include <QIODevice>
#include <QMetaObject>

MediaReceiver::MediaReceiver(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<QByteArray>("QByteArray");
    qRegisterMetaType<quint8>("quint8");
    qRegisterMetaType<quint32>("quint32");

    m_videoDecodeWorker = new VideoDecodeWorker;
    m_videoDecodeWorker->moveToThread(&m_videoDecodeThread);
    connect(&m_videoDecodeThread, &QThread::finished,
            m_videoDecodeWorker, &QObject::deleteLater);
    connect(m_videoDecodeWorker, &VideoDecodeWorker::frameDecoded,
            this, &MediaReceiver::onVideoFrameDecoded, Qt::QueuedConnection);
    connect(m_videoDecodeWorker, &VideoDecodeWorker::decodeFailed,
            this, &MediaReceiver::onVideoDecodeFailed, Qt::QueuedConnection);
    m_videoDecodeThread.setObjectName(QStringLiteral("VideoDecodeWorkerThread"));
    m_videoDecodeThread.start();
}

MediaReceiver::~MediaReceiver()
{
    m_videoDecodeThread.quit();
    m_videoDecodeThread.wait(1500);
}

void MediaReceiver::onPacketReceived(const QByteArray &packet)
{
    if (packet.isEmpty()) {
        return;
    }

    QDataStream stream(packet);
    stream.setVersion(QDataStream::Qt_6_0);

    quint8 version = 0;
    quint8 streamTypeRaw = 0;
    quint8 sourceKindRaw = 0;
    quint16 flags = 0;
    quint32 streamId = 0;
    quint64 timestampUs = 0;
    quint32 sequence = 0;
    quint32 payloadSize = 0;

    stream >> version
           >> streamTypeRaw
           >> sourceKindRaw
           >> flags
           >> streamId
           >> timestampUs
           >> sequence
           >> payloadSize;

    Q_UNUSED(flags);
    Q_UNUSED(streamId);
    Q_UNUSED(timestampUs);
    Q_UNUSED(sequence);

    if (stream.status() != QDataStream::Ok || version != 1) {
        emit receiverMessage(QStringLiteral("收到无法解析的数据包"));
        return;
    }

    QByteArray payload;
    payload.resize(static_cast<int>(payloadSize));
    const int readBytes = payload.isEmpty() ? 0 : stream.readRawData(payload.data(), payload.size());
    if (readBytes != payload.size()) {
        emit receiverMessage(QStringLiteral("收到不完整的数据包"));
        return;
    }

    const StreamType streamType = static_cast<StreamType>(streamTypeRaw);

    if (streamType == StreamType::Control) {
        const SourceKind sourceKind = static_cast<SourceKind>(sourceKindRaw);
        if (sourceKind == SourceKind::CameraPip && payload == QByteArray("camera_off")) {
            const quint8 cameraKind = static_cast<quint8>(SourceKind::CameraPip);
            m_ignoreNextCameraDecodedFrame = m_videoDecodeInFlight.value(cameraKind, false);
            m_videoDecodeInFlight[cameraKind] = false;
            m_pendingVideoPayload.remove(cameraKind);
            emit cameraStoppedReceived();
            return;
        }
        emit receiverMessage(QStringLiteral("收到控制消息：%1").arg(QString::fromUtf8(payload)));
        return;
    }

    if (payload.isEmpty()) {
        emit receiverMessage(QStringLiteral("收到空媒体数据包"));
        return;
    }

    if (streamType == StreamType::Video) {
        // JPEG 解码比较重，放到独立线程。若上一帧还没解完，只保留最新帧，避免延迟累积。
        submitVideoForDecode(sourceKindRaw, payload);
        return;
    }

    if (streamType == StreamType::Audio) {
        emit audioFrameReceived(payload);
        return;
    }

    // 批注和控制消息先完成传输链路，第二阶段再渲染到接收端 overlay。
}

void MediaReceiver::onVideoFrameDecoded(quint8 sourceKindRaw, const QImage &image)
{
    if (image.isNull()) {
        submitPendingVideoIfAny(sourceKindRaw);
        return;
    }

    const SourceKind sourceKind = static_cast<SourceKind>(sourceKindRaw);
    if (sourceKind == SourceKind::CameraPip) {
        if (m_ignoreNextCameraDecodedFrame) {
            m_ignoreNextCameraDecodedFrame = false;
            submitPendingVideoIfAny(sourceKindRaw);
            return;
        }
        emit cameraFrameReceived(image);
    } else {
        emit mainVideoFrameReceived(image);
    }

    submitPendingVideoIfAny(sourceKindRaw);
}

void MediaReceiver::submitVideoForDecode(quint8 sourceKindRaw, const QByteArray &payload)
{
    if (!m_videoDecodeWorker || payload.isEmpty()) {
        return;
    }

    if (m_videoDecodeInFlight.value(sourceKindRaw, false)) {
        m_pendingVideoPayload.insert(sourceKindRaw, payload);
        return;
    }

    m_videoDecodeInFlight[sourceKindRaw] = true;
    QMetaObject::invokeMethod(m_videoDecodeWorker,
                              "decodeFrame",
                              Qt::QueuedConnection,
                              Q_ARG(quint8, sourceKindRaw),
                              Q_ARG(QByteArray, payload));
}

void MediaReceiver::submitPendingVideoIfAny(quint8 sourceKindRaw)
{
    m_videoDecodeInFlight[sourceKindRaw] = false;
    if (!m_pendingVideoPayload.contains(sourceKindRaw)) {
        return;
    }

    const QByteArray nextPayload = m_pendingVideoPayload.take(sourceKindRaw);
    submitVideoForDecode(sourceKindRaw, nextPayload);
}

void MediaReceiver::onVideoDecodeFailed(quint8 sourceKindRaw, const QString &message)
{
    if (sourceKindRaw == static_cast<quint8>(SourceKind::CameraPip)) {
        m_ignoreNextCameraDecodedFrame = false;
    }
    emit receiverMessage(message);
    submitPendingVideoIfAny(sourceKindRaw);
}
