#include <QtTest/QtTest>

#include "network/sender.h"

#include <QDataStream>
#include <QVector>

namespace {

QByteArray makeAudioFrame(qint16 sample = 600, int sampleCount = 320)
{
    QByteArray pcm(sampleCount * static_cast<int>(sizeof(qint16)), Qt::Uninitialized);
    auto* values = reinterpret_cast<qint16*>(pcm.data());
    for (int i = 0; i < sampleCount; ++i) {
        values[i] = sample;
    }
    return pcm;
}

QImage makeVideoFrame()
{
    QImage image(QSize(160, 90), QImage::Format_RGB32);
    image.fill(Qt::blue);
    return image;
}

StrokePacket makeStrokePacket()
{
    StrokePacket pkt;
    pkt.type = StrokeEventType::Begin;
    pkt.strokeId = 7;
    pkt.point = QPointF(10.0, 12.0);
    pkt.color = Qt::red;
    pkt.width = 3.0f;
    pkt.isEraser = false;
    return pkt;
}

struct RecordedPacket {
    PacketHeader header;
    bool reliable{false};
};

class MockTransport final : public INetworkTransport
{
public:
    bool sendPacket(const QByteArray& packet, bool reliable) override
    {
        QDataStream stream(packet);
        stream.setVersion(QDataStream::Qt_6_0);

        RecordedPacket recorded;
        stream >> recorded.header.version
               >> recorded.header.streamType
               >> recorded.header.flags
               >> recorded.header.streamId
               >> recorded.header.timestampUs
               >> recorded.header.sequence
               >> recorded.header.payloadSize;
        recorded.reliable = reliable;
        packets.append(recorded);
        return true;
    }

    QVector<RecordedPacket> packets;
};

} // namespace

class SenderQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void sendsHigherPriorityPacketsFirst();
    void stopPreventsFurtherDelivery();
};

void SenderQueueTest::sendsHigherPriorityPacketsFirst()
{
    Sender sender;
    MockTransport transport;
    sender.setTransport(&transport);
    QVERIFY(sender.start());

    sender.onMainScreenFrameCaptured(makeVideoFrame());
    sender.onStrokePacketReady(makeStrokePacket());
    sender.onAudioDataReady(makeAudioFrame());

    QTRY_COMPARE(transport.packets.size(), 3);
    QCOMPARE(transport.packets.at(0).header.streamType, static_cast<quint8>(StreamType::Audio));
    QCOMPARE(transport.packets.at(1).header.streamType, static_cast<quint8>(StreamType::Annotation));
    QCOMPARE(transport.packets.at(2).header.streamType, static_cast<quint8>(StreamType::Video));

    sender.stop();
}

void SenderQueueTest::stopPreventsFurtherDelivery()
{
    Sender sender;
    MockTransport transport;
    sender.setTransport(&transport);
    QVERIFY(sender.start());

    sender.onAudioDataReady(makeAudioFrame());
    QTRY_COMPARE(transport.packets.size(), 1);

    sender.stop();
    const int sentBeforeStop = transport.packets.size();

    sender.onMainScreenFrameCaptured(makeVideoFrame());
    sender.onAudioDataReady(makeAudioFrame());
    sender.onStrokePacketReady(makeStrokePacket());

    QTest::qWait(50);
    QCOMPARE(transport.packets.size(), sentBeforeStop);
}

QTEST_GUILESS_MAIN(SenderQueueTest)

#include "test_sender_queue.moc"
