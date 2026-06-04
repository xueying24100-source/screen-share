#pragma once

#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>

// A macOS shareable source that can be handed to the Qt client layer.
// The frontend only needs to keep `id` + `type` to identify a source.
struct ScreenCaptureSourceInfo {
    enum class SourceType {
        Display = 0,
        Window = 1
    };

    //启动采集靠id+type确定来源
    quint32 id = 0;
    SourceType type = SourceType::Display;
    QString name;
    QSize size;
    float scale = 1.0f;//缩放比例
};

Q_DECLARE_METATYPE(ScreenCaptureSourceInfo)
Q_DECLARE_METATYPE(QVector<ScreenCaptureSourceInfo>)

//预设分辨率
enum class CaptureResolutionPreset {
    Native = 0,//原始分辨率
    Half,//原始分辨率的一半
    HD720,//HD720p，1280x720
    HD1080//HD1080p，1920x1080
};

//设计方式：PImpl，可以把具体实现藏起来
class ScreenCaptureManager : public QObject {
    Q_OBJECT
public:
    explicit ScreenCaptureManager(QObject *parent = nullptr);
    ~ScreenCaptureManager() override;

    void setIncludeCurrentApplicationContent(bool include);
    bool includeCurrentApplicationContent() const;

    void setResolutionPreset(CaptureResolutionPreset preset);
    CaptureResolutionPreset resolutionPreset() const;

    void setCapturesAudio(bool enabled);
    bool capturesAudio() const;

    // Enumerate all active displays visible to the current user session.
    QVector<ScreenCaptureSourceInfo> enumerateDisplays();

    // Enumerate shareable top-level windows. Current app windows are optional.
    QVector<ScreenCaptureSourceInfo> enumerateWindows();

    // Optional preview/capture helpers kept for local validation.
    bool startCapture(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType);
    void stopCapture(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType);
    void stopAllCaptures();

signals:
    void displaysEnumerated(const QVector<ScreenCaptureSourceInfo> &displays);
    void windowsEnumerated(const QVector<ScreenCaptureSourceInfo> &windows);
    void frameCaptured(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType, const QImage &frame);
    void audioDataCaptured(quint32 sourceId,
                           ScreenCaptureSourceInfo::SourceType sourceType,
                           const QByteArray &audioData,
                           int sampleRate,
                           int channelCount);
    void captureError(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType, const QString &error);
    void captureStopped(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType);

private:
    bool refreshShareableContent(QString *errorMessage);

    class Impl;
    Impl *d;
};
