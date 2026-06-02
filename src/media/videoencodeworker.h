#pragma once

#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QSize>

class VideoEncodeWorker : public QObject
{
    Q_OBJECT
public:
    explicit VideoEncodeWorker(QObject *parent = nullptr);

public slots:
    void encodeFrame(quint32 streamId,
                     quint8 sourceKind,
                     const QImage &frame,
                     int quality,
                     const QSize &maxSize);

signals:
    void frameEncoded(quint32 streamId, quint8 sourceKind, const QByteArray &payload);
};
