#include "audioplayer.h"

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
{
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

QAudioFormat AudioPlayer::defaultFormat() const
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

void AudioPlayer::start()
{
    if (m_running) {
        return;
    }

    const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull()) {
        emit playerError("No default audio output device found");
        return;
    }

    const QAudioFormat format = defaultFormat();
    if (!outputDevice.isFormatSupported(format)) {
        emit playerError("Default audio output does not support 16kHz mono 16-bit PCM");
        return;
    }

    m_audioSink = new QAudioSink(outputDevice, format, this);
    m_audioDevice = m_audioSink->start();
    if (!m_audioDevice) {
        emit playerError("Failed to start audio playback");
        delete m_audioSink;
        m_audioSink = nullptr;
        return;
    }

    m_running = true;
}

void AudioPlayer::stop()
{
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    m_audioDevice = nullptr;
    m_running = false;
}

void AudioPlayer::playData(const QByteArray& data)
{
    if (!m_running || !m_audioDevice || data.isEmpty()) {
        return;
    }

    const qint64 written = m_audioDevice->write(data);
    if (written < 0) {
        emit playerError("Failed to write audio data to output device");
    }
}
