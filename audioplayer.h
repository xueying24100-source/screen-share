#pragma once
#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QMediaDevices>
#include <QByteArray>
#include <QString>

class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer();

    void start();
    void stop();

public slots:
    void playData(const QByteArray& data);

signals:
    void playerError(const QString& error);

private:
    QAudioSink*  m_audioSink   = nullptr;
    QIODevice*   m_audioDevice = nullptr;
    bool         m_running     = false;

    QAudioFormat defaultFormat() const;
};
