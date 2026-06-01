#include "videodecodeworker.h"

VideoDecodeWorker::VideoDecodeWorker(QObject *parent)
    : QObject(parent)
{
}

void VideoDecodeWorker::decodeFrame(quint8 sourceKind, const QByteArray &payload)
{
    if (payload.isEmpty()) {
        emit decodeFailed(sourceKind, QStringLiteral("收到空视频帧"));
        return;
    }

    QImage image;
    if (!image.loadFromData(payload, "JPEG")) {
        emit decodeFailed(sourceKind, QStringLiteral("视频帧解码失败"));
        return;
    }

    emit frameDecoded(sourceKind, image);
}
