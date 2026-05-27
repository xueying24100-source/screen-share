#pragma once

#include <QObject>
#include <QThread>

class SystemAudioCapturerWorker;

class SystemAudioCapturer : public QObject
{
    Q_OBJECT
public:
    explicit SystemAudioCapturer(QObject* parent = nullptr);
    ~SystemAudioCapturer();

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

signals:
    void systemAudioDataReady(const QByteArray& pcm, int sampleRate, int channels);
    void captureError(const QString& error);

private:
    void startWorker();
    void stopWorker();

    bool m_enabled{false};
    QThread m_thread;
    SystemAudioCapturerWorker* m_worker{nullptr};
};
