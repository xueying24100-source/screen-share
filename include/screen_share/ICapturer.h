#pragma once

#include <QObject>
#include <QString>

#include "screen_share/Types.h"

namespace ss {

class ICapturer : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool start(const SourceInfo& src, const CaptureConfig& cfg) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

signals:
    void frameReady(const ss::VideoFrame& frame);
    void audioReady(const ss::AudioFrame& frame);
    void errorOccurred(const QString& msg);
};

} // namespace ss
