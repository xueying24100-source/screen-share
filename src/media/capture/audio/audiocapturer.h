#pragma once
#include <QObject>
#include <QAudioInput>
#include <QIODevice>
#include <QAudioSource>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QByteArray>
#include <QString>

class AudioCapturer : public QObject
{
    Q_OBJECT
public:
    explicit AudioCapturer(QObject* parent = nullptr);
    ~AudioCapturer();

    bool isRunning() const { return m_running; }
    bool isMuted() const { return m_muted; }

public slots:
    void start();
    void stop();
    void setMuted(bool muted);

signals:
    void audioDataReady(const QByteArray& data);
    void captureError(const QString& error);

private slots:
    void onDataReady();

private:
    QAudioSource*  m_audioSource  = nullptr;
    QIODevice*     m_audioDevice  = nullptr;
    bool           m_running      = false;
    bool           m_muted        = false;

    QAudioFormat defaultFormat() const;
};
