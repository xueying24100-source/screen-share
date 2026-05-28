#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QQueue>
#include <QScopedPointer>
#include <QTimer>

#include "annotationoverlay.h"

// Sender v3 skeleton: extensible media hub for Feishu-meeting style evolution.
// Future upgrades: H.264/Opus codecs, real network transports, multi-stream PiP policies.

enum class StreamType : quint8 { Video = 0, Audio = 1, Annotation = 2, Control = 3 };
enum class SourceKind : quint8 {
    DesktopMain = 0,
    CameraPip = 1,
    Microphone = 2,
    AnnotationStroke = 3,
    AnnotationText = 4,
    SystemControl = 5
};
enum class SendPriority : quint8 { Critical = 0, High = 1, Normal = 2, Low = 3 };
using StreamId = quint32;

struct PacketHeader {
    quint8  version = 1;
    quint8  streamType = 0;
    quint16 flags = 0;
    quint32 streamId = 0;
    quint64 timestampUs = 0;
    quint32 sequence = 0;
    quint32 payloadSize = 0;
};

class INetworkTransport
{
public:
    virtual ~INetworkTransport() = default;
    virtual bool sendPacket(const QByteArray& packet, bool reliable) = 0;
};

class IVideoEncoder
{
public:
    virtual ~IVideoEncoder() = default;
    virtual QByteArray encode(const QImage& frame, int quality) = 0;
};

class JpegVideoEncoder : public IVideoEncoder
{
public:
    QByteArray encode(const QImage& frame, int quality) override;
};

class IAudioEncoder
{
public:
    virtual ~IAudioEncoder() = default;
    virtual QByteArray encode(const QByteArray& pcm) = 0;
};

class PcmAudioEncoder : public IAudioEncoder
{
public:
    QByteArray encode(const QByteArray& pcm) override;
};

class AnnotationSerializer
{
public:
    static QByteArray serializeStroke(const StrokePacket& pkt);
    static QByteArray serializeText(const TextAnnotation& text);
};

class DebugTransport : public INetworkTransport
{
public:
    bool sendPacket(const QByteArray& packet, bool reliable) override;
};

class Sender : public QObject
{
    Q_OBJECT
public:
    explicit Sender(QObject* parent = nullptr);

    void setTransport(INetworkTransport* transport);

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    StreamId registerVideoStream(SourceKind sourceKind,
                                 SendPriority priority = SendPriority::Normal,
                                 bool enabled = true,
                                 int maxFps = 30,
                                 int videoQuality = 75);
    StreamId registerAudioStream(SourceKind sourceKind,
                                 SendPriority priority = SendPriority::Critical,
                                 bool enabled = true);
    StreamId registerAnnotationStream(SourceKind sourceKind,
                                      SendPriority priority = SendPriority::High,
                                      bool enabled = true);

    void unregisterStream(StreamId streamId);

    void setTargetBitrate(quint32 bps);
    void setMaxFps(StreamId streamId, int fps);
    void setVideoQuality(StreamId streamId, int quality);
    void setStreamEnabled(StreamId streamId, bool enabled);

    StreamId mainVideoStreamId() const { return m_mainVideoStreamId; }
    StreamId pipVideoStreamId() const { return m_pipVideoStreamId; }
    StreamId audioStreamId() const { return m_audioStreamId; }
    StreamId annotationStrokeStreamId() const { return m_annotationStrokeStreamId; }
    StreamId annotationTextStreamId() const { return m_annotationTextStreamId; }

public slots:
    void onMainScreenFrameCaptured(const QImage& frame);
    void onPipFrameCaptured(const QImage& frame);
    void onAudioDataReady(const QByteArray& data);
    void onStrokePacketReady(const StrokePacket& pkt);
    void onTextAnnotationCreated(const TextAnnotation& text);

private slots:
    void processSendLoop();

private:
    struct OutboundStream {
        StreamId     streamId = 0;
        StreamType   streamType = StreamType::Video;
        SourceKind   sourceKind = SourceKind::DesktopMain;
        SendPriority priority = SendPriority::Normal;
        bool         enabled = true;
        int          maxFps = 30;
        int          videoQuality = 75;
        quint64      lastFrameTimestampUs = 0;
        quint32      nextSequence = 1;
    };

    struct QueuedPacket {
        StreamId    streamId = 0;
        SourceKind  sourceKind = SourceKind::SystemControl;
        QByteArray  packet;
        bool        reliable = false;
    };

    StreamId registerStreamInternal(StreamType streamType,
                                    SourceKind sourceKind,
                                    SendPriority priority,
                                    bool enabled,
                                    int maxFps,
                                    int videoQuality);
    StreamId findStreamId(SourceKind sourceKind) const;
    quint64 nowUs() const;

    void enqueueVideoFrame(SourceKind sourceKind, const QImage& frame);
    void enqueuePayload(StreamId streamId, const QByteArray& payload, bool reliable);
    QByteArray buildPacket(OutboundStream& stream, const QByteArray& payload, bool reliable, quint16 flags = 0);
    void enqueueByPriority(const QueuedPacket& packet, SendPriority priority);
    bool tryDequeueNext(QueuedPacket& out);
    void dropOldVideoPacketsForStream(StreamId streamId, QQueue<QueuedPacket>& queue);

    INetworkTransport* m_transport = nullptr;
    QScopedPointer<IVideoEncoder> m_videoEncoder;
    QScopedPointer<IAudioEncoder> m_audioEncoder;

    QHash<StreamId, OutboundStream> m_streams;
    StreamId m_nextStreamId = 1;

    StreamId m_mainVideoStreamId = 0;
    StreamId m_pipVideoStreamId = 0;
    StreamId m_audioStreamId = 0;
    StreamId m_annotationStrokeStreamId = 0;
    StreamId m_annotationTextStreamId = 0;

    quint32 m_targetBitrateBps = 2 * 1000 * 1000;
    bool m_running = false;

    QTimer m_sendTimer;

    QQueue<QueuedPacket> m_criticalQueue;
    QQueue<QueuedPacket> m_highQueue;
    QQueue<QueuedPacket> m_normalQueue;
    QQueue<QueuedPacket> m_lowQueue;
};
