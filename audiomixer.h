#pragma once

#include <QByteArray>
#include <QObject>

class AudioMixer : public QObject
{
    Q_OBJECT
public:
    explicit AudioMixer(QObject* parent = nullptr);

    void pushMicPcm(const QByteArray& pcm16k1chInt16);
    void pushSystemPcm(const QByteArray& pcmFloat32Interleaved, int sampleRate, int channels);
    void reset();

signals:
    void mixedAudioReady(const QByteArray& pcm);

private:
    static constexpr int kFrameBytes = 640;
    static constexpr int kFrameSamples = 320;
    static constexpr int kTargetSampleRate = 16000;
    static constexpr qint64 kInactiveThresholdMs = 100;
    static constexpr int kBufferOverflowBytes = 16000 * 2 * 2;
    static constexpr int kMaxFramesPerCall = 5;

    void tryEmitFrames();
    void trimBufferIfOverflow(QByteArray& buffer, const char* bufferName);
    QByteArray takeFrame(QByteArray& buffer);
    QByteArray mixFrames(const QByteArray& micFrame, const QByteArray& systemFrame) const;
    QByteArray convertSystemTo16kMonoInt16(const QByteArray& pcmFloat32Interleaved, int sampleRate, int channels) const;
    static qint16 floatToInt16(float sample);

    QByteArray m_micBuffer;
    QByteArray m_systemBuffer;
    qint64 m_lastMicDataMs{-1};
    qint64 m_lastSystemDataMs{-1};
    bool m_firstMicLogged{false};
    bool m_firstSystemLogged{false};
};
