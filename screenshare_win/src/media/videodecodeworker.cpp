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
    // v8.2 起主共享可以使用 PNG 无损帧，摄像头仍可使用 JPEG。
    // 这里不强制写死 JPEG，让 Qt 自动识别格式。
    if (!image.loadFromData(payload)) {
        emit decodeFailed(sourceKind, QStringLiteral("视频帧解码失败"));
        return;
    }

    emit frameDecoded(sourceKind, image);
}
