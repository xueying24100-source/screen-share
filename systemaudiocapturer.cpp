#include "systemaudiocapturer.h"

#include <QByteArray>
#include <QDebug>
#include <QTimerEvent>

#ifdef Q_OS_WIN
#include <QMetaObject>
#include <QTimer>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <avrt.h>

class SystemAudioCapturerWorker : public QObject
{
    Q_OBJECT
public:
    explicit SystemAudioCapturerWorker(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

public slots:
    void start()
    {
        if (m_running) {
            return;
        }

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_comInitialized = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            emit error(QStringLiteral("CoInitializeEx failed: 0x%1").arg(QString::number(static_cast<quint32>(hr), 16)));
            return;
        }

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                              nullptr,
                              CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator),
                              reinterpret_cast<void**>(&m_deviceEnumerator));
        if (FAILED(hr)) {
            emit error(QStringLiteral("MMDeviceEnumerator failed"));
            if (m_comInitialized) CoUninitialize();
            return;
        }

        hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
        if (FAILED(hr) || !m_device) {
            emit error(QStringLiteral("GetDefaultAudioEndpoint failed"));
            cleanup();
            if (m_comInitialized) CoUninitialize();
            return;
        }

        hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_audioClient));
        if (FAILED(hr) || !m_audioClient) {
            emit error(QStringLiteral("Activate IAudioClient failed"));
            cleanup();
            if (m_comInitialized) CoUninitialize();
            return;
        }

        hr = m_audioClient->GetMixFormat(&m_mixFormat);
        if (FAILED(hr) || !m_mixFormat) {
            emit error(QStringLiteral("GetMixFormat failed"));
            cleanup();
            if (m_comInitialized) CoUninitialize();
            return;
        }

        REFERENCE_TIME hnsBufferDuration = 10000000; // 1s
        hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                       AUDCLNT_STREAMFLAGS_LOOPBACK,
                                       hnsBufferDuration,
                                       0,
                                       m_mixFormat,
                                       nullptr);
        if (FAILED(hr)) {
            emit error(QStringLiteral("IAudioClient::Initialize(loopback) failed"));
            cleanup();
            if (m_comInitialized) CoUninitialize();
            return;
        }

        hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&m_captureClient));
        if (FAILED(hr) || !m_captureClient) {
            emit error(QStringLiteral("GetService IAudioCaptureClient failed"));
            cleanup();
            if (m_comInitialized) CoUninitialize();
            return;
        }

        DWORD taskIndex = 0;
        m_mmcssHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        hr = m_audioClient->Start();
        if (FAILED(hr)) {
            emit error(QStringLiteral("IAudioClient::Start failed"));
            cleanup();
            if (m_comInitialized) CoUninitialize();
            return;
        }

        m_running = true;
        m_timer.start(10, this);
    }

    void stop()
    {
        if (!m_running) {
            cleanup();
            if (m_comInitialized) {
                CoUninitialize();
                m_comInitialized = false;
            }
            return;
        }
        m_running = false;
        killTimer(m_timer.timerId());

        if (m_audioClient) {
            m_audioClient->Stop();
        }

        if (m_mmcssHandle) {
            AvRevertMmThreadCharacteristics(m_mmcssHandle);
            m_mmcssHandle = nullptr;
        }

        cleanup();
        if (m_comInitialized) {
            CoUninitialize();
            m_comInitialized = false;
        }
    }

signals:
    void audioReady(const QByteArray& pcm, int sampleRate, int channels);
    void error(const QString& err);

protected:
    void timerEvent(QTimerEvent* event) override
    {
        if (!m_running || event->timerId() != m_timer.timerId() || !m_captureClient || !m_mixFormat) {
            return;
        }

        UINT32 packetLength = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) {
            emit error(QStringLiteral("GetNextPacketSize failed"));
            return;
        }

        while (packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            hr = m_captureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                emit error(QStringLiteral("GetBuffer failed"));
                return;
            }

            const int blockAlign = m_mixFormat->nBlockAlign;
            const int bytes = static_cast<int>(numFrames * blockAlign);
            QByteArray pcm;
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0) {
                pcm = QByteArray(bytes, 0);
            } else if (pData && bytes > 0) {
                pcm = QByteArray(reinterpret_cast<const char*>(pData), bytes);
            }

            if (!pcm.isEmpty()) {
                emit audioReady(pcm, static_cast<int>(m_mixFormat->nSamplesPerSec), static_cast<int>(m_mixFormat->nChannels));
            }

            m_captureClient->ReleaseBuffer(numFrames);

            hr = m_captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                emit error(QStringLiteral("GetNextPacketSize failed"));
                return;
            }
        }
    }

private:
    void cleanup()
    {
        if (m_mixFormat) {
            CoTaskMemFree(m_mixFormat);
            m_mixFormat = nullptr;
        }
        if (m_captureClient) {
            m_captureClient->Release();
            m_captureClient = nullptr;
        }
        if (m_audioClient) {
            m_audioClient->Release();
            m_audioClient = nullptr;
        }
        if (m_device) {
            m_device->Release();
            m_device = nullptr;
        }
        if (m_deviceEnumerator) {
            m_deviceEnumerator->Release();
            m_deviceEnumerator = nullptr;
        }
    }

    bool m_running{false};
    QBasicTimer m_timer;
    IMMDeviceEnumerator* m_deviceEnumerator{nullptr};
    IMMDevice* m_device{nullptr};
    IAudioClient* m_audioClient{nullptr};
    IAudioCaptureClient* m_captureClient{nullptr};
    WAVEFORMATEX* m_mixFormat{nullptr};
    HANDLE m_mmcssHandle{nullptr};
    bool m_comInitialized{false};
};

#else
class SystemAudioCapturerWorker : public QObject
{
    Q_OBJECT
public slots:
    void start() {}
    void stop() {}
signals:
    void audioReady(const QByteArray& pcm, int sampleRate, int channels);
    void error(const QString& err);
};
#endif

SystemAudioCapturer::SystemAudioCapturer(QObject* parent)
    : QObject(parent)
{
}

SystemAudioCapturer::~SystemAudioCapturer()
{
    setEnabled(false);
}

void SystemAudioCapturer::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (m_enabled) {
        startWorker();
    } else {
        stopWorker();
    }
}

void SystemAudioCapturer::startWorker()
{
    if (m_worker) {
        return;
    }

    m_worker = new SystemAudioCapturerWorker();
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &SystemAudioCapturerWorker::audioReady, this, &SystemAudioCapturer::systemAudioDataReady);
    connect(m_worker, &SystemAudioCapturerWorker::error, this, &SystemAudioCapturer::captureError);

    m_thread.start();
    QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection);
}

void SystemAudioCapturer::stopWorker()
{
    if (!m_worker) {
        return;
    }

    QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
    m_worker = nullptr;
}

#include "systemaudiocapturer.moc"
