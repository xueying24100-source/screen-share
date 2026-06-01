#pragma once

#include <QObject>
#include <QByteArray>
#include <QImage>

class VideoDecodeWorker : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecodeWorker(QObject *parent = nullptr);

public slots:
    void decodeFrame(quint8 sourceKind, const QByteArray &payload);

signals:
    void frameDecoded(quint8 sourceKind, const QImage &image);
    void decodeFailed(quint8 sourceKind, const QString &message);
};
