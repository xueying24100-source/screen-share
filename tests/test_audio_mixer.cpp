#include <QtTest/QtTest>

#include "media/mixer/audiomixer.h"

#include <QSignalSpy>

namespace {

QByteArray makeInt16Frame(qint16 sample, int sampleCount = 320)
{
    QByteArray pcm(sampleCount * static_cast<int>(sizeof(qint16)), Qt::Uninitialized);
    auto* values = reinterpret_cast<qint16*>(pcm.data());
    for (int i = 0; i < sampleCount; ++i) {
        values[i] = sample;
    }
    return pcm;
}

QByteArray makeFloat32Frame(float sample, int frameCount = 320, int channels = 1)
{
    QByteArray pcm(frameCount * channels * static_cast<int>(sizeof(float)), Qt::Uninitialized);
    auto* values = reinterpret_cast<float*>(pcm.data());
    for (int i = 0; i < frameCount * channels; ++i) {
        values[i] = sample;
    }
    return pcm;
}

qint16 firstSample(const QByteArray& pcm)
{
    return pcm.size() >= static_cast<int>(sizeof(qint16))
        ? reinterpret_cast<const qint16*>(pcm.constData())[0]
        : 0;
}

} // namespace

class AudioMixerTest : public QObject
{
    Q_OBJECT

private slots:
    void micOnlyPassesThrough();
    void systemOnlyProducesOutput();
    void duckingReducesSystemContribution();
    void resetClearsBufferedAudio();
};

void AudioMixerTest::micOnlyPassesThrough()
{
    AudioMixer mixer;
    mixer.setMicGain(1.0);

    QSignalSpy spy(&mixer, &AudioMixer::mixedAudioReady);
    const QByteArray micFrame = makeInt16Frame(1200);
    mixer.pushMicPcm(micFrame);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), micFrame);
}

void AudioMixerTest::systemOnlyProducesOutput()
{
    AudioMixer mixer;
    mixer.setSystemGain(1.0);

    QSignalSpy spy(&mixer, &AudioMixer::mixedAudioReady);
    mixer.pushSystemPcm(makeFloat32Frame(0.2f), 16000, 1);

    QCOMPARE(spy.count(), 1);
    const QByteArray mixed = spy.takeFirst().at(0).toByteArray();
    QVERIFY(!mixed.isEmpty());
    QCOMPARE(mixed.size(), 640);
}

void AudioMixerTest::duckingReducesSystemContribution()
{
    const QByteArray systemFrame = makeFloat32Frame(0.2f);

    AudioMixer baselineMixer;
    baselineMixer.setSystemGain(1.0);
    QSignalSpy baselineSpy(&baselineMixer, &AudioMixer::mixedAudioReady);
    baselineMixer.pushSystemPcm(systemFrame, 16000, 1);
    QCOMPARE(baselineSpy.count(), 1);
    const qint16 baselineSystemSample = firstSample(baselineSpy.takeFirst().at(0).toByteArray());
    QVERIFY(baselineSystemSample > 0);

    AudioMixer mixer;
    mixer.setMicGain(1.0);
    mixer.setSystemGain(1.0);

    QSignalSpy spy(&mixer, &AudioMixer::mixedAudioReady);
    const QByteArray micFrame = makeInt16Frame(4000);
    const QByteArray halfMic = micFrame.left(micFrame.size() / 2);
    const QByteArray halfSystem = systemFrame.left(systemFrame.size() / 2);

    mixer.pushMicPcm(halfMic);
    mixer.pushSystemPcm(halfSystem, 16000, 1);
    mixer.pushMicPcm(micFrame.mid(micFrame.size() / 2));
    mixer.pushSystemPcm(systemFrame.mid(systemFrame.size() / 2), 16000, 1);

    QCOMPARE(spy.count(), 1);
    const QByteArray mixed = spy.takeFirst().at(0).toByteArray();
    const qint16 mixedSample = firstSample(mixed);
    const qint16 systemContribution = static_cast<qint16>(mixedSample - 4000);

    QVERIFY(systemContribution > 0);
    QVERIFY(systemContribution < baselineSystemSample);
}

void AudioMixerTest::resetClearsBufferedAudio()
{
    AudioMixer mixer;

    QSignalSpy spy(&mixer, &AudioMixer::mixedAudioReady);
    const QByteArray micFrame = makeInt16Frame(900);
    const QByteArray halfMic = micFrame.left(micFrame.size() / 2);

    mixer.pushMicPcm(halfMic);
    QCOMPARE(spy.count(), 0);

    mixer.reset();
    mixer.pushMicPcm(halfMic);
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(AudioMixerTest)

#include "test_audio_mixer.moc"
