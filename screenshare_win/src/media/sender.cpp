#include "sender.h"
#include "videoencodeworker.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QIODevice>
#include <QMetaObject>
#include <QtGlobal>

namespace {
constexpr quint8 kPacketVersion = 1;
constexpr quint16 kFlagReliable = 0x1;
}

QByteArray PcmAudioEncoder::encode(const QByteArray& pcm)
{
    return pcm;
}

QByteArray AnnotationSerializer::serializeStroke(const StrokePacket& pkt)
{
    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint8>(pkt.type)
           << pkt.strokeId
           << pkt.point
           << pkt.color
           << pkt.width
           << pkt.isEraser;

    return out;
}

QByteArray AnnotationSerializer::serializeText(const TextAnnotation& text)
{
    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << text.position
           << text.text
           << text.color
           << text.fontSize;

    return out;
}

bool DebugTransport::sendPacket(const QByteArray& packet, bool reliable)
{
    if (packet.isEmpty()) {
        return false;
    }

    QDataStream stream(packet);
    stream.setVersion(QDataStream::Qt_6_0);

    PacketHeader header;
    stream >> header.version
           >> header.streamType
           >> header.sourceKind
           >> header.flags
           >> header.streamId
           >> header.timestampUs
           >> header.sequence
           >> header.payloadSize;

    qDebug().nospace()
        << "[DebugTransport] sendPacket"
        << " streamType=" << header.streamType
        << " sourceKind=" << header.sourceKind
        << " streamId=" << header.streamId
        << " seq=" << header.sequence
        << " payloadSize=" << header.payloadSize
        << " reliable=" << reliable;

    return true;
}

Sender::Sender(QObject* parent)
    : QObject(parent)
    , m_audioEncoder(new PcmAudioEncoder)
{
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<QByteArray>("QByteArray");
    qRegisterMetaType<quint8>("quint8");
    qRegisterMetaType<quint32>("quint32");
    qRegisterMetaType<QSize>("QSize");

    m_sendTimer.setParent(this);
    m_sendTimer.setInterval(8);
    connect(&m_sendTimer, &QTimer::timeout, this, &Sender::processSendLoop);

    m_videoEncodeWorker = new VideoEncodeWorker;
    m_videoEncodeWorker->moveToThread(&m_videoEncodeThread);
    connect(&m_videoEncodeThread, &QThread::finished,
            m_videoEncodeWorker, &QObject::deleteLater);
    connect(m_videoEncodeWorker, &VideoEncodeWorker::frameEncoded,
            this, &Sender::onVideoFrameEncoded, Qt::QueuedConnection);
    m_videoEncodeThread.setObjectName(QStringLiteral("VideoEncodeWorkerThread"));
    m_videoEncodeThread.start();

    m_mainVideoStreamId = registerVideoStream(SourceKind::DesktopMain,
                                              SendPriority::Normal,
                                              true,
                                              8,
                                              100,
                                              QSize());
    m_pipVideoStreamId = registerVideoStream(SourceKind::CameraPip,
                                             SendPriority::Low,
                                             false,
                                             10,
                                             60,
                                             QSize(480, 270));
    m_audioStreamId = registerAudioStream(SourceKind::Microphone, SendPriority::Critical, true);
    m_annotationStrokeStreamId = registerAnnotationStream(SourceKind::AnnotationStroke, SendPriority::High, true);
    m_annotationTextStreamId = registerAnnotationStream(SourceKind::AnnotationText, SendPriority::High, true);
}

Sender::~Sender()
{
    stop();
    m_videoEncodeThread.quit();
    m_videoEncodeThread.wait(1500);
}

void Sender::setTransport(INetworkTransport* transport)
{
    m_transport = transport;
}

bool Sender::start()
{
    if (m_running) {
        return true;
    }

    m_running = true;
    m_sendTimer.start();
    return true;
}

void Sender::stop()
{
    if (!m_running) {
        m_transport = nullptr;
        return;
    }

    m_running = false;
    m_sendTimer.stop();

    m_videoEncodeInFlight.clear();
    m_pendingVideoFrames.clear();
    m_criticalQueue.clear();
    m_highQueue.clear();
    m_normalQueue.clear();
    m_lowQueue.clear();
    m_transport = nullptr;
}

StreamId Sender::registerVideoStream(SourceKind sourceKind,
                                     SendPriority priority,
                                     bool enabled,
                                     int maxFps,
                                     int videoQuality,
                                     QSize maxVideoSize)
{
    return registerStreamInternal(StreamType::Video,
                                  sourceKind,
                                  priority,
                                  enabled,
                                  qMax(1, maxFps),
                                  qBound(1, videoQuality, 100),
                                  maxVideoSize.isValid() ? maxVideoSize : defaultVideoMaxSize(sourceKind));
}

StreamId Sender::registerAudioStream(SourceKind sourceKind,
                                     SendPriority priority,
                                     bool enabled)
{
    return registerStreamInternal(StreamType::Audio, sourceKind, priority, enabled, 0, 0);
}

StreamId Sender::registerAnnotationStream(SourceKind sourceKind,
                                          SendPriority priority,
                                          bool enabled)
{
    return registerStreamInternal(StreamType::Annotation, sourceKind, priority, enabled, 0, 0);
}

void Sender::unregisterStream(StreamId streamId)
{
    m_streams.remove(streamId);
    m_videoEncodeInFlight.remove(streamId);
    m_pendingVideoFrames.remove(streamId);
}

void Sender::setTargetBitrate(quint32 bps)
{
    m_targetBitrateBps = bps;
}

void Sender::setMaxFps(StreamId streamId, int fps)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || it->streamType != StreamType::Video) {
        return;
    }
    it->maxFps = qMax(1, fps);
}

void Sender::setVideoQuality(StreamId streamId, int quality)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || it->streamType != StreamType::Video) {
        return;
    }
    it->videoQuality = qBound(1, quality, 100);
}

void Sender::setVideoMaxSize(StreamId streamId, const QSize& maxSize)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || it->streamType != StreamType::Video) {
        return;
    }
    it->videoMaxSize = maxSize.isValid() ? maxSize : defaultVideoMaxSize(it->sourceKind);
}

void Sender::setStreamEnabled(StreamId streamId, bool enabled)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end()) {
        return;
    }
    it->enabled = enabled;
    if (!enabled) {
        m_videoEncodeInFlight.remove(streamId);
        m_pendingVideoFrames.remove(streamId);
        dropOldVideoPacketsForStream(streamId, m_criticalQueue);
        dropOldVideoPacketsForStream(streamId, m_highQueue);
        dropOldVideoPacketsForStream(streamId, m_normalQueue);
        dropOldVideoPacketsForStream(streamId, m_lowQueue);
    }
}

void Sender::sendCameraStopped()
{
    if (!m_transport) {
        return;
    }

    QByteArray payload("camera_off");

    PacketHeader header;
    header.version = kPacketVersion;
    header.streamType = static_cast<quint8>(StreamType::Control);
    header.sourceKind = static_cast<quint8>(SourceKind::CameraPip);
    header.flags = kFlagReliable;
    header.streamId = m_pipVideoStreamId;
    header.timestampUs = nowUs();
    header.sequence = 0;
    header.payloadSize = static_cast<quint32>(payload.size());

    QByteArray out;
    QDataStream streamOut(&out, QIODevice::WriteOnly);
    streamOut.setVersion(QDataStream::Qt_6_0);
    streamOut << header.version
              << header.streamType
              << header.sourceKind
              << header.flags
              << header.streamId
              << header.timestampUs
              << header.sequence
              << header.payloadSize;
    streamOut.writeRawData(payload.constData(), payload.size());

    m_transport->sendPacket(out, true);
}

void Sender::sendShareStopped()
{
    if (!m_transport) {
        return;
    }

    // 通知远端：当前用户已经结束主共享。远端收到后要清空大窗口，
    // 避免一直停留在最后一帧共享画面。
    QByteArray payload("share_off");

    PacketHeader header;
    header.version = kPacketVersion;
    header.streamType = static_cast<quint8>(StreamType::Control);
    header.sourceKind = static_cast<quint8>(SourceKind::DesktopMain);
    header.flags = kFlagReliable;
    header.streamId = m_mainVideoStreamId;
    header.timestampUs = nowUs();
    header.sequence = 0;
    header.payloadSize = static_cast<quint32>(payload.size());

    QByteArray out;
    QDataStream streamOut(&out, QIODevice::WriteOnly);
    streamOut.setVersion(QDataStream::Qt_6_0);
    streamOut << header.version
              << header.streamType
              << header.sourceKind
              << header.flags
              << header.streamId
              << header.timestampUs
              << header.sequence
              << header.payloadSize;
    streamOut.writeRawData(payload.constData(), payload.size());

    m_transport->sendPacket(out, true);
}

void Sender::onMainScreenFrameCaptured(const QImage& frame)
{
    if (!m_running) {
        return;
    }
    enqueueVideoFrame(SourceKind::DesktopMain, frame);
}

void Sender::onPipFrameCaptured(const QImage& frame)
{
    if (!m_running) {
        return;
    }
    enqueueVideoFrame(SourceKind::CameraPip, frame);
}

void Sender::onAudioDataReady(const QByteArray& data)
{
    if (!m_running) {
        return;
    }
    const StreamId streamId = findStreamId(SourceKind::Microphone);
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled) {
        return;
    }

    const QByteArray payload = m_audioEncoder->encode(data);
    enqueuePayload(streamId, payload, false);
}

void Sender::onStrokePacketReady(const StrokePacket& pkt)
{
    if (!m_running) {
        return;
    }
    const StreamId streamId = findStreamId(SourceKind::AnnotationStroke);
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled) {
        return;
    }

    const QByteArray payload = AnnotationSerializer::serializeStroke(pkt);
    enqueuePayload(streamId, payload, true);
}

void Sender::onTextAnnotationCreated(const TextAnnotation& text)
{
    if (!m_running) {
        return;
    }
    const StreamId streamId = findStreamId(SourceKind::AnnotationText);
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled) {
        return;
    }

    const QByteArray payload = AnnotationSerializer::serializeText(text);
    enqueuePayload(streamId, payload, true);
}

void Sender::processSendLoop()
{
    if (!m_running || !m_transport) {
        return;
    }

    int sentThisTick = 0;
    constexpr int kMaxPacketsPerTick = 6;

    QueuedPacket packet;
    while (sentThisTick < kMaxPacketsPerTick && tryDequeueNext(packet)) {
        m_transport->sendPacket(packet.packet, packet.reliable);
        ++sentThisTick;
    }
}

void Sender::onVideoFrameEncoded(quint32 streamId, quint8 sourceKindRaw, const QByteArray& payload)
{
    Q_UNUSED(sourceKindRaw);
    m_videoEncodeInFlight[streamId] = false;

    if (m_running && !payload.isEmpty()) {
        enqueuePayload(streamId, payload, false);
    }

    if (m_running && m_pendingVideoFrames.contains(streamId) && m_pendingVideoFrames.value(streamId).valid) {
        submitPendingVideoFrame(streamId);
    }
}

StreamId Sender::registerStreamInternal(StreamType streamType,
                                        SourceKind sourceKind,
                                        SendPriority priority,
                                        bool enabled,
                                        int maxFps,
                                        int videoQuality,
                                        const QSize& maxVideoSize)
{
    const StreamId existingId = findStreamId(sourceKind);
    if (existingId != 0) {
        auto it = m_streams.find(existingId);
        if (it != m_streams.end()) {
            it->priority = priority;
            it->enabled = enabled;
            if (streamType == StreamType::Video) {
                it->maxFps = maxFps;
                it->videoQuality = videoQuality;
                it->videoMaxSize = maxVideoSize.isValid() ? maxVideoSize : defaultVideoMaxSize(sourceKind);
            }
        }
        return existingId;
    }

    OutboundStream stream;
    stream.streamId = m_nextStreamId++;
    stream.streamType = streamType;
    stream.sourceKind = sourceKind;
    stream.priority = priority;
    stream.enabled = enabled;
    stream.maxFps = maxFps;
    stream.videoQuality = videoQuality;
    stream.videoMaxSize = maxVideoSize.isValid() ? maxVideoSize : defaultVideoMaxSize(sourceKind);

    m_streams.insert(stream.streamId, stream);
    return stream.streamId;
}

StreamId Sender::findStreamId(SourceKind sourceKind) const
{
    for (auto it = m_streams.constBegin(); it != m_streams.constEnd(); ++it) {
        if (it->sourceKind == sourceKind) {
            return it.key();
        }
    }
    return 0;
}

quint64 Sender::nowUs() const
{
    return static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
}

QSize Sender::defaultVideoMaxSize(SourceKind sourceKind) const
{
    if (sourceKind == SourceKind::CameraPip) {
        return QSize(480, 270);
    }
    // 主共享默认不再限制分辨率，避免远端画面被二次缩小导致文字发糊。
    // 需要低带宽时由“流畅模式”显式设置较小的 maxSize。
    return QSize();
}

void Sender::enqueueVideoFrame(SourceKind sourceKind, const QImage& frame)
{
    if (!m_running || frame.isNull()) {
        return;
    }
    const StreamId streamId = findStreamId(sourceKind);
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled) {
        return;
    }

    if (it->maxFps > 0) {
        const quint64 now = nowUs();
        const quint64 frameIntervalUs = 1000000ULL / static_cast<quint64>(it->maxFps);
        if (it->lastFrameTimestampUs != 0 && now - it->lastFrameTimestampUs < frameIntervalUs) {
            return;
        }
        it->lastFrameTimestampUs = now;
    }

    submitVideoFrameForEncoding(*it, frame);
}

void Sender::submitVideoFrameForEncoding(OutboundStream& stream, const QImage& frame)
{
    if (!m_videoEncodeWorker) {
        return;
    }

    if (m_videoEncodeInFlight.value(stream.streamId, false)) {
        PendingVideoFrame pending;
        pending.sourceKind = stream.sourceKind;
        pending.frame = frame;
        pending.quality = stream.videoQuality;
        pending.maxSize = stream.videoMaxSize.isValid() ? stream.videoMaxSize : defaultVideoMaxSize(stream.sourceKind);
        pending.valid = true;
        m_pendingVideoFrames.insert(stream.streamId, pending);
        return;
    }

    m_videoEncodeInFlight[stream.streamId] = true;
    QMetaObject::invokeMethod(m_videoEncodeWorker,
                              "encodeFrame",
                              Qt::QueuedConnection,
                              Q_ARG(quint32, stream.streamId),
                              Q_ARG(quint8, static_cast<quint8>(stream.sourceKind)),
                              Q_ARG(QImage, frame),
                              Q_ARG(int, stream.videoQuality),
                              Q_ARG(QSize, stream.videoMaxSize.isValid() ? stream.videoMaxSize : defaultVideoMaxSize(stream.sourceKind)));
}

void Sender::submitPendingVideoFrame(StreamId streamId)
{
    PendingVideoFrame pending = m_pendingVideoFrames.take(streamId);
    if (!pending.valid || pending.frame.isNull()) {
        return;
    }

    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled) {
        return;
    }

    m_videoEncodeInFlight[streamId] = true;
    QMetaObject::invokeMethod(m_videoEncodeWorker,
                              "encodeFrame",
                              Qt::QueuedConnection,
                              Q_ARG(quint32, streamId),
                              Q_ARG(quint8, static_cast<quint8>(pending.sourceKind)),
                              Q_ARG(QImage, pending.frame),
                              Q_ARG(int, pending.quality),
                              Q_ARG(QSize, pending.maxSize.isValid() ? pending.maxSize : defaultVideoMaxSize(pending.sourceKind)));
}

void Sender::enqueuePayload(StreamId streamId, const QByteArray& payload, bool reliable)
{
    if (!m_running || payload.isEmpty()) {
        return;
    }

    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled) {
        return;
    }

    QByteArray packetData = buildPacket(*it, payload, reliable);

    QueuedPacket packet;
    packet.streamId = streamId;
    packet.sourceKind = it->sourceKind;
    packet.packet = std::move(packetData);
    packet.reliable = reliable;

    enqueueByPriority(packet, it->priority);
}

QByteArray Sender::buildPacket(OutboundStream& stream,
                               const QByteArray& payload,
                               bool reliable,
                               quint16 flags)
{
    PacketHeader header;
    header.version = kPacketVersion;
    header.streamType = static_cast<quint8>(stream.streamType);
    header.sourceKind = static_cast<quint8>(stream.sourceKind);
    header.flags = static_cast<quint16>(flags | (reliable ? kFlagReliable : 0));
    header.streamId = stream.streamId;
    header.timestampUs = nowUs();
    header.sequence = stream.nextSequence++;
    header.payloadSize = static_cast<quint32>(payload.size());

    QByteArray out;
    QDataStream streamOut(&out, QIODevice::WriteOnly);
    streamOut.setVersion(QDataStream::Qt_6_0);

    streamOut << header.version
              << header.streamType
              << header.sourceKind
              << header.flags
              << header.streamId
              << header.timestampUs
              << header.sequence
              << header.payloadSize;

    streamOut.writeRawData(payload.constData(), payload.size());
    return out;
}

void Sender::enqueueByPriority(const QueuedPacket& packet, SendPriority priority)
{
    switch (priority) {
    case SendPriority::Critical:
        m_criticalQueue.enqueue(packet);
        break;
    case SendPriority::High:
        m_highQueue.enqueue(packet);
        break;
    case SendPriority::Normal:
        dropOldVideoPacketsForStream(packet.streamId, m_normalQueue);
        m_normalQueue.enqueue(packet);
        break;
    case SendPriority::Low:
        dropOldVideoPacketsForStream(packet.streamId, m_lowQueue);
        m_lowQueue.enqueue(packet);
        break;
    }
}

bool Sender::tryDequeueNext(QueuedPacket& out)
{
    if (!m_criticalQueue.isEmpty()) {
        out = m_criticalQueue.dequeue();
        return true;
    }
    if (!m_highQueue.isEmpty()) {
        out = m_highQueue.dequeue();
        return true;
    }
    if (!m_normalQueue.isEmpty()) {
        out = m_normalQueue.dequeue();
        return true;
    }
    if (!m_lowQueue.isEmpty()) {
        out = m_lowQueue.dequeue();
        return true;
    }
    return false;
}

void Sender::dropOldVideoPacketsForStream(StreamId streamId, QQueue<QueuedPacket>& queue)
{
    if (queue.isEmpty()) {
        return;
    }

    QQueue<QueuedPacket> filtered;
    while (!queue.isEmpty()) {
        QueuedPacket packet = queue.dequeue();
        if (packet.streamId != streamId) {
            filtered.enqueue(packet);
        }
    }
    queue = std::move(filtered);
}
