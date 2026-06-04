#include "videoencodeworker.h"

#include <QBuffer>
#include <QIODevice>
#include <QtGlobal>

namespace {
QImage scaledForNetwork(const QImage &frame, const QSize &maxSize)
{
    if (frame.isNull()) {
        return {};
    }

    QImage image = frame;
    if (image.format() != QImage::Format_RGB32 &&
        image.format() != QImage::Format_ARGB32 &&
        image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_RGB32);
    }

    if (maxSize.isValid() &&
        (image.width() > maxSize.width() || image.height() > maxSize.height())) {
        image = image.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}
}

VideoEncodeWorker::VideoEncodeWorker(QObject *parent)
    : QObject(parent)
{
}

void VideoEncodeWorker::encodeFrame(quint32 streamId,
                                    quint8 sourceKind,
                                    const QImage &frame,
                                    int quality,
                                    const QSize &maxSize)
{
    const QImage image = scaledForNetwork(frame, maxSize);
    if (image.isNull()) {
        emit frameEncoded(streamId, sourceKind, QByteArray());
        return;
    }

    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    const int clampedQuality = qBound(1, quality, 100);
    if (clampedQuality >= 100) {
        // 主共享普通模式使用 PNG 无损编码，避免文字和 UI 边缘被 JPEG 压糊。
        // 缺点是数据量更大；需要更流畅时可勾选“流畅模式”退回 JPEG。
        image.save(&buffer, "PNG");
    } else {
        image.save(&buffer, "JPEG", clampedQuality);
    }

    emit frameEncoded(streamId, sourceKind, encoded);
}
