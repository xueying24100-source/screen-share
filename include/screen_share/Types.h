#pragma once

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <QSize>
#include <QString>
#include <QVector>

namespace ss {

struct SourceInfo {
    enum Type {
        Display,
        Window
    };

    quint64 id = 0;
    Type type = Display;
    QString name;
    QSize size;
    float scale = 1.0f;
};

struct CaptureConfig {
    int fps = 15;
    QSize outputSize{1280, 720};
    bool captureCursor = true;
    bool captureAudio = false;
};

struct VideoFrame {
    QImage image;
    qint64 ptsMs = 0;
    quint64 sourceId = 0;
};

struct AudioFrame {
    QByteArray pcm;
    int sampleRate = 48000;
    int channels = 2;
    qint64 ptsMs = 0;
};

} // namespace ss

Q_DECLARE_METATYPE(ss::SourceInfo)
Q_DECLARE_METATYPE(ss::CaptureConfig)
Q_DECLARE_METATYPE(ss::VideoFrame)
Q_DECLARE_METATYPE(ss::AudioFrame)
Q_DECLARE_METATYPE(QVector<ss::SourceInfo>)
