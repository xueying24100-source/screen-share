#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>

class AudioMixer : public QObject
{
    Q_OBJECT
public:
    explicit AudioMixer(QObject* parent = nullptr);

    void setMicGain(double gain);
    void setSystemGain(double gain);
    void setDuckingEnabled(bool on);

public slots:
    void pushMicPcm(const QByteArray& pcm16k1chInt16);
    void pushSystemPcm(const QByteArray& pcmFloat32Interleaved, int sampleRate, int channels);
    void reset();

signals:
    void mixedAudioReady(const QByteArray& pcm);

private:
    static constexpr int kTargetSampleRate = 16000;
    static constexpr int kFrameMs = 20;
    static constexpr int kFrameSamples = kTargetSampleRate * kFrameMs / 1000;
    static constexpr int kFrameBytes = 640;
    static constexpr qint64 kInactiveThresholdMs = 200;
    static constexpr int kSingleBufferHardLimitMs = 500;
    static constexpr int kSingleBufferDropMs = 250;
    static constexpr int kSilenceFillTriggerMs = 60;
    static constexpr double kSpeechThresholdDbFs = -30.0;
    static constexpr double kDuckingGainRatio = 0.3;
    static constexpr double kMicDbEmaWindowMs = 200.0;
    static constexpr int kMaxFramesPerCall = 5;
    static constexpr double kMinGain = 0.0;
    static constexpr double kMaxGain = 2.0;
    static constexpr double kDefaultMicGain = 0.7;
    static constexpr double kDefaultSystemGain = 0.5;

    void tryEmitFrames();
    void trimBufferIfOverflow(QByteArray& buffer, const char* bufferName);
    QByteArray takeFrame(QByteArray& buffer);
    QByteArray mixFrames(const QByteArray& micFrame, const QByteArray& systemFrame) const;
    QByteArray applyGainToFrame(const QByteArray& frame, double gain) const;
    QByteArray makeSilentFrame() const;
    QByteArray convertSystemTo16kMonoInt16(const QByteArray& pcmFloat32Interleaved, int sampleRate, int channels) const;
    double calculateInt16DbFs(const QByteArray& pcm) const;
    static int msToBytes(int ms);
    void updateMicSpeechEma(double packetDbFs, int packetBytes);
    void logStatusThrottled();
    static qint16 floatToInt16(float sample);

    QByteArray m_micBuffer;
    QByteArray m_systemBuffer;
    qint64 m_lastMicDataMs{-1};
    qint64 m_lastSystemDataMs{-1};
    bool m_firstMicLogged{false};
    bool m_firstSystemLogged{false};
    double m_micGain{kDefaultMicGain};
    double m_systemGain{kDefaultSystemGain};
    bool m_duckingEnabled{true};
    double m_smoothedMicDbFs{-90.0};
    double m_lastMicDbFs{-90.0};
    double m_lastSystemDbFs{-90.0};
    QElapsedTimer m_statusLogTimer;
    QElapsedTimer m_overflowLogTimer;
};
