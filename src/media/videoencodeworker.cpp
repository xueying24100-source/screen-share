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
        image = image.scaled(maxSize, Qt::KeepAspectRatio, Qt::FastTransformation);
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
    image.save(&buffer, "JPEG", clampedQuality);

    emit frameEncoded(streamId, sourceKind, encoded);
}
