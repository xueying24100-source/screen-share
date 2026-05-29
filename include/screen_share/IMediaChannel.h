#pragma once

#include <QObject>

#include "screen_share/Types.h"

namespace ss {

class IMediaChannel : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void sendVideo(const VideoFrame& f) = 0;
    virtual void sendAudio(const AudioFrame& f) = 0;

signals:
    void videoArrived(const ss::VideoFrame& f);
    void audioArrived(const ss::AudioFrame& f);
};

} // namespace ss
