#include "audiomixer.h"

#include <QDateTime>
#include <QDebug>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <vector>

AudioMixer::AudioMixer(QObject* parent)
    : QObject(parent)
{
}

void AudioMixer::pushMicPcm(const QByteArray& pcm16k1chInt16)
{
    if (pcm16k1chInt16.isEmpty()) {
        return;
    }

    if (!m_firstMicLogged) {
        qDebug() << "[Mixer] first mic packet bytes=" << pcm16k1chInt16.size();
        m_firstMicLogged = true;
    }
    trimBufferIfOverflow(m_micBuffer, "mic");
    m_lastMicDataMs = QDateTime::currentMSecsSinceEpoch();
    m_micBuffer.append(pcm16k1chInt16);
    tryEmitFrames();
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

    trimBufferIfOverflow(m_systemBuffer, "system");
    m_lastSystemDataMs = QDateTime::currentMSecsSinceEpoch();
    m_systemBuffer.append(converted);
    tryEmitFrames();
}

void AudioMixer::reset()
{
    m_micBuffer.clear();
    m_systemBuffer.clear();
    m_lastMicDataMs = -1;
    m_lastSystemDataMs = -1;
    m_firstMicLogged = false;
    m_firstSystemLogged = false;
    qDebug() << "[Mixer] reset";
}

void AudioMixer::tryEmitFrames()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool micActive = (m_lastMicDataMs >= 0) && ((nowMs - m_lastMicDataMs) <= kInactiveThresholdMs);
    const bool systemActive = (m_lastSystemDataMs >= 0) && ((nowMs - m_lastSystemDataMs) <= kInactiveThresholdMs);
    int emittedFrames = 0;

    while (emittedFrames < kMaxFramesPerCall) {
        if (micActive && systemActive) {
            if (m_micBuffer.size() < kFrameBytes || m_systemBuffer.size() < kFrameBytes) {
                return;
            }
            emit mixedAudioReady(mixFrames(takeFrame(m_micBuffer), takeFrame(m_systemBuffer)));
            ++emittedFrames;
            continue;
        }

        if (micActive) {
            if (m_micBuffer.size() < kFrameBytes) {
                return;
            }
            emit mixedAudioReady(takeFrame(m_micBuffer));
            ++emittedFrames;
            continue;
        }

        if (systemActive) {
            if (m_systemBuffer.size() < kFrameBytes) {
                return;
            }
            emit mixedAudioReady(takeFrame(m_systemBuffer));
            ++emittedFrames;
            continue;
        }

        return;
    }
}

void AudioMixer::trimBufferIfOverflow(QByteArray& buffer, const char* bufferName)
{
    if (buffer.size() <= kBufferOverflowBytes) {
        return;
    }

    int dropBytes = buffer.size() / 2;
    dropBytes -= (dropBytes % kFrameBytes);
    if (dropBytes <= 0) {
        return;
    }

    buffer.remove(0, dropBytes);
    const int droppedMs = dropBytes / 32;
    qDebug() << "AudioMixer" << bufferName << "buffer overflow, dropped" << droppedMs << "ms";
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

    for (int i = 0; i < kFrameSamples; ++i) {
        const int sum = static_cast<int>(micSamples[i]) + static_cast<int>(systemSamples[i]);
        outSamples[i] = static_cast<qint16>(qBound(static_cast<int>(std::numeric_limits<qint16>::min()),
                                                   sum,
                                                   static_cast<int>(std::numeric_limits<qint16>::max())));
    }

    return out;
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

qint16 AudioMixer::floatToInt16(float sample)
{
    const float clamped = qBound(-1.0f, sample, 1.0f);
    const int scaled = static_cast<int>(std::lround(clamped * 32767.0f));
    return static_cast<qint16>(qBound(static_cast<int>(std::numeric_limits<qint16>::min()),
                                      scaled,
                                      static_cast<int>(std::numeric_limits<qint16>::max())));
}
