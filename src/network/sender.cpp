#include "network/sender.h"

#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QIODevice>

namespace {
constexpr quint8 kPacketVersion = 1;
constexpr quint16 kFlagReliable = 0x1;
}

QByteArray JpegVideoEncoder::encode(const QImage& frame, int quality)
{
    if (frame.isNull()) {
        return {};
    }

    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    const int clampedQuality = qBound(1, quality, 100);
    if (!frame.save(&buffer, "JPEG", clampedQuality)) {
        return {};
    }
    return encoded;
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
           >> header.flags
           >> header.streamId
           >> header.timestampUs
           >> header.sequence
           >> header.payloadSize;

    qDebug().nospace()
        << "[DebugTransport] sendPacket"
        << " streamType=" << header.streamType
        << " streamId=" << header.streamId
        << " seq=" << header.sequence
        << " payloadSize=" << header.payloadSize
        << " reliable=" << reliable;

    return true;
}

Sender::Sender(QObject* parent)
    : QObject(parent)
    , m_videoEncoder(new JpegVideoEncoder)
    , m_audioEncoder(new PcmAudioEncoder)
{
    m_sendTimer.setInterval(5);
    connect(&m_sendTimer, &QTimer::timeout, this, &Sender::processSendLoop);

    m_mainVideoStreamId = registerVideoStream(SourceKind::DesktopMain, SendPriority::Normal, true, 30, 75);
    m_pipVideoStreamId = registerVideoStream(SourceKind::CameraPip, SendPriority::Low, false, 15, 70);
    m_audioStreamId = registerAudioStream(SourceKind::Microphone, SendPriority::Critical, true);
    m_annotationStrokeStreamId = registerAnnotationStream(SourceKind::AnnotationStroke, SendPriority::High, true);
    m_annotationTextStreamId = registerAnnotationStream(SourceKind::AnnotationText, SendPriority::High, true);
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
        return;
    }

    m_running = false;
    m_sendTimer.stop();

    m_criticalQueue.clear();
    m_highQueue.clear();
    m_normalQueue.clear();
    m_lowQueue.clear();
}

StreamId Sender::registerVideoStream(SourceKind sourceKind,
                                     SendPriority priority,
                                     bool enabled,
                                     int maxFps,
                                     int videoQuality)
{
    return registerStreamInternal(StreamType::Video,
                                  sourceKind,
                                  priority,
                                  enabled,
                                  qMax(1, maxFps),
                                  qBound(1, videoQuality, 100));
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
}

void Sender::setTargetBitrate(quint32 bps)
{
    m_targetBitrateBps = bps;
}

void Sender::setMaxFps(StreamId streamId, int fps)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end()) {
        return;
    }
    if (it->streamType != StreamType::Video) {
        return;
    }
    it->maxFps = qMax(1, fps);
}

void Sender::setVideoQuality(StreamId streamId, int quality)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end()) {
        return;
    }
    if (it->streamType != StreamType::Video) {
        return;
    }
    it->videoQuality = qBound(1, quality, 100);
}

void Sender::setStreamEnabled(StreamId streamId, bool enabled)
{
    auto it = m_streams.find(streamId);
    if (it == m_streams.end()) {
        return;
    }
    it->enabled = enabled;
}

void Sender::onMainScreenFrameCaptured(const QImage& frame)
{
    enqueueVideoFrame(SourceKind::DesktopMain, frame);
}

void Sender::onPipFrameCaptured(const QImage& frame)
{
    enqueueVideoFrame(SourceKind::CameraPip, frame);
}

void Sender::onAudioDataReady(const QByteArray& data)
{
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
    constexpr int kMaxPacketsPerTick = 8;

    QueuedPacket packet;
    while (sentThisTick < kMaxPacketsPerTick && tryDequeueNext(packet)) {
        m_transport->sendPacket(packet.packet, packet.reliable);
        ++sentThisTick;
    }
}

StreamId Sender::registerStreamInternal(StreamType streamType,
                                        SourceKind sourceKind,
                                        SendPriority priority,
                                        bool enabled,
                                        int maxFps,
                                        int videoQuality)
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

void Sender::enqueueVideoFrame(SourceKind sourceKind, const QImage& frame)
{
    const StreamId streamId = findStreamId(sourceKind);
    auto it = m_streams.find(streamId);
    if (it == m_streams.end() || !it->enabled || frame.isNull()) {
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

    const QByteArray payload = m_videoEncoder->encode(frame, it->videoQuality);
    enqueuePayload(streamId, payload, false);
}

void Sender::enqueuePayload(StreamId streamId, const QByteArray& payload, bool reliable)
{
    if (payload.isEmpty()) {
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
