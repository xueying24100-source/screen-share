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

    void start();
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void audioDataReady(const QByteArray& data);
    void captureError(const QString& error);

private slots:
    void onDataReady();

private:
    QAudioSource*  m_audioSource  = nullptr;
    QIODevice*     m_audioDevice  = nullptr;
    bool           m_running      = false;

    QAudioFormat defaultFormat() const;
};
