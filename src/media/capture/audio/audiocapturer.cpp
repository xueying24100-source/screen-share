#include "media/capture/audio/audiocapturer.h"

#include <QDebug>
#include <QThread>

AudioCapturer::AudioCapturer(QObject* parent)
    : QObject(parent)
{
}

AudioCapturer::~AudioCapturer()
{
    stop();
}

QAudioFormat AudioCapturer::defaultFormat() const
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

void AudioCapturer::start()
{
    if (m_running) {
        return;
    }

    qDebug() << "[AudioCapturer] thread=" << QThread::currentThread();

    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        emit captureError("No default audio input device found");
        return;
    }

    const QAudioFormat format = defaultFormat();
    if (!inputDevice.isFormatSupported(format)) {
        emit captureError("Default audio input does not support 16kHz mono 16-bit PCM");
        return;
    }

    m_audioSource = new QAudioSource(inputDevice, format, this);

    // Request a large buffer to tolerate main-thread load (~200 ms).
    // The driver may silently clamp; we log the actual value after start().
    m_audioSource->setBufferSize(6400);

    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        emit captureError("Failed to start audio capture");
        delete m_audioSource;
        m_audioSource = nullptr;
        return;
    }

    qDebug() << "[AudioCapturer] started, actual bufferSize=" << m_audioSource->bufferSize();
    connect(m_audioDevice, &QIODevice::readyRead, this, &AudioCapturer::onDataReady);
    m_running = true;
}

void AudioCapturer::stop()
{
    if (m_audioDevice) {
        disconnect(m_audioDevice, &QIODevice::readyRead, this, &AudioCapturer::onDataReady);
        m_audioDevice = nullptr;
    }

    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
    }

    m_running = false;
}

void AudioCapturer::setMuted(bool muted)
{
    m_muted = muted;
}

void AudioCapturer::onDataReady()
{
    if (!m_audioDevice || m_muted) {
        return;
    }

    const QByteArray data = m_audioDevice->readAll();
    if (!data.isEmpty()) {
        emit audioDataReady(data);
    }
}
