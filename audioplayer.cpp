#include "audioplayer.h"

#include <QDateTime>
#include <QDebug>

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
    start(defaultFormat());
}

void AudioPlayer::start(const QAudioFormat& format)
{
    if (m_running) {
        return;
    }

    const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull()) {
        emit playerError("No default audio output device found");
        return;
    }

    if (!outputDevice.isFormatSupported(format)) {
        emit playerError("Default audio output does not support requested audio format");
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
    qDebug() << "[AudioPlayer] start sr=" << format.sampleRate() << "ch=" << format.channelCount();
}

void AudioPlayer::stop()
{
    qDebug() << "[AudioPlayer] stop";
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
    static qint64 s_lastPlayDataLogMs = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (s_lastPlayDataLogMs == 0 || (nowMs - s_lastPlayDataLogMs) >= 1000) {
        s_lastPlayDataLogMs = nowMs;
        qDebug() << "[AudioPlayer] playData bytes=" << data.size() << "running=" << m_running;
    }

    if (!m_running || !m_audioDevice || data.isEmpty()) {
        return;
    }

    const qint64 written = m_audioDevice->write(data);
    if (written < 0) {
        emit playerError("Failed to write audio data to output device");
    }
}
