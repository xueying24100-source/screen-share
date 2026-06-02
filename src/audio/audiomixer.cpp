#include "audiomixer.h"

#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <vector>

AudioMixer::AudioMixer(QObject* parent)
    : QObject(parent)
{
}

void AudioMixer::setMicGain(double gain)
{
    m_micGain = qBound(kMinGain, gain, kMaxGain);
}

void AudioMixer::setSystemGain(double gain)
{
    m_systemGain = qBound(kMinGain, gain, kMaxGain);
}

void AudioMixer::setDuckingEnabled(bool on)
{
    m_duckingEnabled = on;
}

void AudioMixer::pushMicPcm(const QByteArray& pcm16k1chInt16)
{
    if (pcm16k1chInt16.isEmpty()) {
        return;
    }

    if (!m_firstMicLogged) {
        qDebug() << "[Mixer] thread=" << QThread::currentThread()
                 << "first mic packet bytes=" << pcm16k1chInt16.size();
        m_firstMicLogged = true;
    }
    m_lastMicDataMs = QDateTime::currentMSecsSinceEpoch();
    m_micBuffer.append(pcm16k1chInt16);
    m_lastMicDbFs = calculateInt16DbFs(pcm16k1chInt16);
    updateMicSpeechEma(m_lastMicDbFs, pcm16k1chInt16.size());
    trimBufferIfOverflow(m_micBuffer, "mic");
    tryEmitFrames();
    logStatusThrottled();
}

void AudioMixer::pushSystemPcm(const QByteArray& pcmFloat32Interleaved, int sampleRate, int channels)
{
    if (pcmFloat32Interleaved.isEmpty()) {
        return;
    }

    if (!m_firstSystemLogged) {
        qDebug() << "[Mixer] first system packet bytes=" << pcmFloat32Interleaved.size();
        m_firstSystemLogged = true;
    }
    const QByteArray converted = convertSystemTo16kMonoInt16(pcmFloat32Interleaved, sampleRate, channels);
    if (converted.isEmpty()) {
        return;
    }

    m_lastSystemDataMs = QDateTime::currentMSecsSinceEpoch();
    m_systemBuffer.append(converted);
    m_lastSystemDbFs = calculateInt16DbFs(converted);
    trimBufferIfOverflow(m_systemBuffer, "system");
    tryEmitFrames();
    logStatusThrottled();
}

void AudioMixer::reset()
{
    m_micBuffer.clear();
    m_systemBuffer.clear();
    m_lastMicDataMs = -1;
    m_lastSystemDataMs = -1;
    m_firstMicLogged = false;
    m_firstSystemLogged = false;
    m_smoothedMicDbFs = -90.0;
    m_lastMicDbFs = -90.0;
    m_lastSystemDbFs = -90.0;
    m_statusLogTimer.invalidate();
    m_overflowLogTimer.invalidate();
    qDebug() << "[Mixer] reset";
}

void AudioMixer::tryEmitFrames()
{
    int emittedFrames = 0;

    while (emittedFrames < kMaxFramesPerCall) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const bool micActive = (m_lastMicDataMs >= 0) && ((nowMs - m_lastMicDataMs) <= kInactiveThresholdMs);
        const bool systemActive = (m_lastSystemDataMs >= 0) && ((nowMs - m_lastSystemDataMs) <= kInactiveThresholdMs);

        if (micActive && systemActive) {
            if (m_micBuffer.size() >= kFrameBytes && m_systemBuffer.size() >= kFrameBytes) {
                emit mixedAudioReady(mixFrames(takeFrame(m_micBuffer), takeFrame(m_systemBuffer)));
                ++emittedFrames;
                continue;
            }

            if (m_micBuffer.size() >= msToBytes(kSilenceFillTriggerMs) && m_systemBuffer.size() < kFrameBytes) {
                emit mixedAudioReady(mixFrames(takeFrame(m_micBuffer), makeSilentFrame()));
                ++emittedFrames;
                continue;
            }

            if (m_systemBuffer.size() >= msToBytes(kSilenceFillTriggerMs) && m_micBuffer.size() < kFrameBytes) {
                emit mixedAudioReady(mixFrames(makeSilentFrame(), takeFrame(m_systemBuffer)));
                ++emittedFrames;
                continue;
            }

            break;
        }

        if (micActive) {
            if (m_micBuffer.size() < kFrameBytes) {
                break;
            }
            emit mixedAudioReady(applyGainToFrame(takeFrame(m_micBuffer), m_micGain));
            ++emittedFrames;
            continue;
        }

        if (systemActive) {
            if (m_systemBuffer.size() < kFrameBytes) {
                break;
            }
            emit mixedAudioReady(applyGainToFrame(takeFrame(m_systemBuffer), m_systemGain));
            ++emittedFrames;
            continue;
        }

        break;
    }
}

void AudioMixer::trimBufferIfOverflow(QByteArray& buffer, const char* bufferName)
{
    const int hardLimitBytes = msToBytes(kSingleBufferHardLimitMs);
    if (buffer.size() <= hardLimitBytes) {
        return;
    }

    int dropBytes = msToBytes(kSingleBufferDropMs);
    dropBytes -= (dropBytes % kFrameBytes);
    if (dropBytes <= 0) {
        return;
    }

    buffer.remove(0, dropBytes);
    const int droppedMs = dropBytes / 32;
    if (!m_overflowLogTimer.isValid() || m_overflowLogTimer.elapsed() >= 1000) {
        m_overflowLogTimer.restart();
        qDebug() << "AudioMixer" << bufferName << "buffer overflow, dropped" << droppedMs << "ms";
    }
}

QByteArray AudioMixer::takeFrame(QByteArray& buffer)
{
    const QByteArray frame = buffer.first(kFrameBytes);
    buffer.remove(0, kFrameBytes);
    return frame;
}

QByteArray AudioMixer::mixFrames(const QByteArray& micFrame, const QByteArray& systemFrame) const
{
    QByteArray out(kFrameBytes, 0);

    const auto* micSamples = reinterpret_cast<const qint16*>(micFrame.constData());
    const auto* systemSamples = reinterpret_cast<const qint16*>(systemFrame.constData());
    auto* outSamples = reinterpret_cast<qint16*>(out.data());
    double effectiveSystemGain = m_systemGain;
    if (m_duckingEnabled && m_smoothedMicDbFs > kSpeechThresholdDbFs) {
        effectiveSystemGain *= kDuckingGainRatio;
    }

    for (int i = 0; i < kFrameSamples; ++i) {
        const double mic = static_cast<double>(micSamples[i]) * m_micGain;
        const double sys = static_cast<double>(systemSamples[i]) * effectiveSystemGain;
        const int sum = static_cast<int>(std::lround(mic + sys));
        outSamples[i] = static_cast<qint16>(qBound(static_cast<int>(std::numeric_limits<qint16>::min()),
                                                   sum,
                                                   static_cast<int>(std::numeric_limits<qint16>::max())));
    }

    return out;
}

QByteArray AudioMixer::applyGainToFrame(const QByteArray& frame, double gain) const
{
    QByteArray out(kFrameBytes, 0);
    const auto* inSamples = reinterpret_cast<const qint16*>(frame.constData());
    auto* outSamples = reinterpret_cast<qint16*>(out.data());
    const double boundedGain = qBound(kMinGain, gain, kMaxGain);

    for (int i = 0; i < kFrameSamples; ++i) {
        const int value = static_cast<int>(std::lround(static_cast<double>(inSamples[i]) * boundedGain));
        outSamples[i] = static_cast<qint16>(qBound(static_cast<int>(std::numeric_limits<qint16>::min()),
                                                   value,
                                                   static_cast<int>(std::numeric_limits<qint16>::max())));
    }
    return out;
}

QByteArray AudioMixer::makeSilentFrame() const
{
    return QByteArray(kFrameBytes, 0);
}

QByteArray AudioMixer::convertSystemTo16kMonoInt16(const QByteArray& pcmFloat32Interleaved, int sampleRate, int channels) const
{
    if (sampleRate <= 0 || channels <= 0) {
        return {};
    }

    const int totalFloatCount = pcmFloat32Interleaved.size() / static_cast<int>(sizeof(float));
    if (totalFloatCount < channels) {
        return {};
    }

    const int frameCount = totalFloatCount / channels;
    if (frameCount <= 0) {
        return {};
    }

    std::vector<float> monoSamples(static_cast<size_t>(frameCount));
    const auto* src = reinterpret_cast<const float*>(pcmFloat32Interleaved.constData());
    for (int frame = 0; frame < frameCount; ++frame) {
        double sum = 0.0;
        for (int ch = 0; ch < channels; ++ch) {
            sum += src[frame * channels + ch];
        }
        monoSamples[static_cast<size_t>(frame)] = static_cast<float>(sum / static_cast<double>(channels));
    }

    const int outFrameCount = qMax(1, static_cast<int>(std::llround(static_cast<double>(frameCount) * kTargetSampleRate / sampleRate)));
    QByteArray out(outFrameCount * static_cast<int>(sizeof(qint16)), 0);
    auto* outSamples = reinterpret_cast<qint16*>(out.data());

    if (frameCount == 1) {
        const qint16 sample = floatToInt16(monoSamples.front());
        for (int i = 0; i < outFrameCount; ++i) {
            outSamples[i] = sample;
        }
        return out;
    }

    if (outFrameCount == 1) {
        outSamples[0] = floatToInt16(monoSamples.front());
        return out;
    }

    for (int i = 0; i < outFrameCount; ++i) {
        const double srcPosition = static_cast<double>(i) * (frameCount - 1) / (outFrameCount - 1);
        const int leftIndex = static_cast<int>(std::floor(srcPosition));
        const int rightIndex = qMin(frameCount - 1, leftIndex + 1);
        const double ratio = srcPosition - leftIndex;
        const float left = monoSamples[static_cast<size_t>(leftIndex)];
        const float right = monoSamples[static_cast<size_t>(rightIndex)];
        const float interp = static_cast<float>(left + (right - left) * ratio);
        outSamples[i] = floatToInt16(interp);
    }

    return out;
}

double AudioMixer::calculateInt16DbFs(const QByteArray& pcm) const
{
    const int sampleCount = pcm.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0) {
        return -90.0;
    }

    const auto* samples = reinterpret_cast<const qint16*>(pcm.constData());
    double sumSquares = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const double sample = static_cast<double>(samples[i]);
        sumSquares += sample * sample;
    }

    const double rms = std::sqrt(sumSquares / static_cast<double>(sampleCount));
    if (rms <= 0.0) {
        return -90.0;
    }
    return 20.0 * std::log10(rms / 32768.0);
}

int AudioMixer::msToBytes(int ms)
{
    return qMax(0, ms) * kTargetSampleRate * static_cast<int>(sizeof(qint16)) / 1000;
}

void AudioMixer::updateMicSpeechEma(double packetDbFs, int packetBytes)
{
    const double packetMs = static_cast<double>(qMax(0, packetBytes)) / 32.0;
    const double alpha = 1.0 - std::exp(-packetMs / kMicDbEmaWindowMs);
    if (m_smoothedMicDbFs <= -89.0 || !std::isfinite(m_smoothedMicDbFs)) {
        m_smoothedMicDbFs = packetDbFs;
        return;
    }
    m_smoothedMicDbFs = (1.0 - alpha) * m_smoothedMicDbFs + alpha * packetDbFs;
}

void AudioMixer::logStatusThrottled()
{
    if (!m_statusLogTimer.isValid() || m_statusLogTimer.elapsed() >= 1000) {
        m_statusLogTimer.restart();
        const bool duckingActive = m_duckingEnabled && (m_smoothedMicDbFs > kSpeechThresholdDbFs);
        qDebug() << "[Mixer] mic RMS=" << m_lastMicDbFs
                 << "dBFS, sys RMS=" << m_lastSystemDbFs
                 << "dBFS, ducking=" << (duckingActive ? "on" : "off")
                 << ", buffers mic=" << (m_micBuffer.size() / 32)
                 << "ms sys=" << (m_systemBuffer.size() / 32) << "ms";
    }
}

qint16 AudioMixer::floatToInt16(float sample)
{
    const float clamped = qBound(-1.0f, sample, 1.0f);
    const int scaled = static_cast<int>(std::lround(clamped * 32767.0f));
    return static_cast<qint16>(qBound(static_cast<int>(std::numeric_limits<qint16>::min()),
                                      scaled,
                                      static_cast<int>(std::numeric_limits<qint16>::max())));
}
