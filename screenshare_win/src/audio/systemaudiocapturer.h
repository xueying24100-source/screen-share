#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>

class SystemAudioCapturerWorker;

class SystemAudioCapturer : public QObject
{
    Q_OBJECT
public:
    explicit SystemAudioCapturer(QObject* parent = nullptr);
    ~SystemAudioCapturer();

    bool isEnabled() const { return m_enabled; }

public slots:
    void setEnabled(bool enabled);

signals:
    void systemAudioDataReady(const QByteArray& pcm, int sampleRate, int channels);
    void captureError(const QString& error);

private:
    void startWorker();
    void stopWorker();

    bool m_enabled{false};
    QThread m_thread;
    QPointer<SystemAudioCapturerWorker> m_worker;
};
