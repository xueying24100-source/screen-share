#include "meetingmainwindow.h"

#include "annotationwindow.h"
#include "audiocapturer.h"
#include "audiomixer.h"
#include "audioplayer.h"
#include "localpreviewwindow.h"
#include "screencapturer.h"
#include "sender.h"
#include "sharetoolbar.h"
#include "systemaudiocapturer.h"

#include <QApplication>
#include <QGuiApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QMessageBox>
#include <QMediaDevices>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {

double calculateMicDbFs(const QByteArray& pcm)
{
    const int sampleCount = pcm.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0) {
        return -90.0;
    }

    const auto* samples = reinterpret_cast<const qint16*>(pcm.constData());
    double sumSquares = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const double sample = static_cast<double>(samples[i]);
        sumSquares += sample * sample;
    }

    const double rms = std::sqrt(sumSquares / sampleCount);
    if (rms <= 0.0) {
        return -90.0;
    }

    return 20.0 * std::log10(rms / 32768.0);
}

double calculateSystemDbFs(const QByteArray& pcm, int sampleRate, int channels)
{
    Q_UNUSED(sampleRate);
    if (channels <= 0) {
        return -90.0;
    }

    const int totalFloatCount = pcm.size() / static_cast<int>(sizeof(float));
    if (totalFloatCount < channels) {
        return -90.0;
    }

    const int frameCount = totalFloatCount / channels;
    if (frameCount <= 0) {
        return -90.0;
    }

    const auto* samples = reinterpret_cast<const float*>(pcm.constData());
    double sumSquares = 0.0;
    for (int frame = 0; frame < frameCount; ++frame) {
        double mono = 0.0;
        for (int ch = 0; ch < channels; ++ch) {
            mono += samples[frame * channels + ch];
        }
        mono /= channels;
        sumSquares += mono * mono;
    }

    const double rms = std::sqrt(sumSquares / frameCount);
    if (rms <= 0.0) {
        return -90.0;
    }

    return 20.0 * std::log10(rms);
}

} // namespace

MeetingMainWindow::MeetingMainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_capturer(new ScreenCapturer(this))
    , m_sender(new Sender(this))
    , m_audioCapturer(new AudioCapturer(this))
    , m_systemAudioCapturer(new SystemAudioCapturer(this))
    , m_audioMixer(new AudioMixer(this))
    , m_localPlayback(new AudioPlayer(this))
    , m_toolbar(new ShareToolbar())
    , m_windowFollowTimer(new QTimer(this))
    , m_previewRefreshTimer(new QTimer(this))
{
    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(18);

    auto* title = new QLabel(QStringLiteral("会议主面板"), central);
    title->setStyleSheet(QStringLiteral("font-size:24px;font-weight:600;"));
    rootLayout->addWidget(title, 0, Qt::AlignCenter);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(12);

    m_shareButton = new QPushButton(QStringLiteral("共享屏幕"), central);
    m_endButton = new QPushButton(QStringLiteral("结束"), central);
    m_endButton->setEnabled(false);

    m_shareButton->setMinimumHeight(52);
    m_endButton->setMinimumHeight(52);

    buttonRow->addWidget(m_shareButton);
    buttonRow->addWidget(m_endButton);
    rootLayout->addLayout(buttonRow);

    setCentralWidget(central);

    connect(m_shareButton, &QPushButton::clicked, this, [this]() {
        ShareSourcePicker picker(this);
        if (picker.exec() != QDialog::Accepted) {
            return;
        }
        startSharing(picker.selection());
    });
    connect(m_endButton, &QPushButton::clicked, this, &MeetingMainWindow::stopSharing);

    connect(m_toolbar, &ShareToolbar::pauseToggled, this, [this](bool paused) {
        if (paused) {
            m_capturer->pause();
        } else {
            m_capturer->resume();
        }
    });
    connect(m_toolbar, &ShareToolbar::annotationToggled, this, [this](bool enabled) {
        if (enabled) {
            if (!m_annotationWindow) {
                m_annotationWindow = new AnnotationWindow();
                connect(m_annotationWindow, &AnnotationWindow::strokePacketReady,
                        m_sender, &Sender::onStrokePacketReady);
                connect(m_annotationWindow, &AnnotationWindow::textAnnotationCreated,
                        m_sender, &Sender::onTextAnnotationCreated);
                connect(m_annotationWindow, &AnnotationWindow::contentChanged,
                        this, [this]() {
                            m_annotationLayerDirty = true;
                            refreshPreviewComposite();
                        });
                connect(m_annotationWindow, &AnnotationWindow::closed, this, [this]() {
                    if (!m_annotationWindow) {
                        return;
                    }
                    m_annotationWindow->deleteLater();
                    m_annotationWindow = nullptr;
                    m_annotationLayerCache = QImage{};
                    m_annotationLayerCacheSourceSize = QSize{};
                    m_annotationLayerDirty = true;
                    m_toolbar->setAnnotationEnabled(false);
                    refreshPreviewComposite();
                });
            }
            applyAnnotationGeometry();
            m_annotationWindow->show();
            m_annotationWindow->raise();
            m_annotationWindow->activateWindow();
            return;
        }

        if (m_annotationWindow) {
            m_annotationWindow->close();
        }
    });
    connect(m_toolbar, &ShareToolbar::micMuteToggled, this, [this](bool muted) {
        m_audioCapturer->setMuted(muted);
    });
    connect(m_toolbar, &ShareToolbar::systemAudioToggled, this, [this](bool enabled) {
        m_systemAudioCapturer->setEnabled(enabled);
    });
    connect(m_toolbar, &ShareToolbar::backRequested, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(m_toolbar, &ShareToolbar::stopRequested, this, &MeetingMainWindow::stopSharing);

    connect(m_windowFollowTimer, &QTimer::timeout, this, &MeetingMainWindow::applyAnnotationGeometry);
    m_previewRefreshTimer->setSingleShot(true);
    m_previewRefreshTimer->setInterval(30);
    connect(m_previewRefreshTimer, &QTimer::timeout, this, [this]() {
        if (!m_preview || m_lastRawFrame.isNull()) {
            return;
        }
        m_preview->updateFrame(composeFrameWithAnnotations(m_lastRawFrame));
    });

    connect(m_capturer, &ScreenCapturer::frameCaptured, this, &MeetingMainWindow::onFrameCaptured);
    connect(m_capturer, &ScreenCapturer::frameMetadataChanged, this, [this](const CaptureFrameMetadata& meta) {
        m_lastFrameBackend = meta.backendName;
        if (m_preview) {
            m_preview->updateMetadata(meta);
        }
    });
    connect(m_capturer, &ScreenCapturer::captureError, this, &MeetingMainWindow::handleCaptureError);
    connect(m_audioCapturer, &AudioCapturer::audioDataReady, m_sender, &Sender::onAudioDataReady);
    connect(m_audioCapturer, &AudioCapturer::audioDataReady, m_audioMixer, &AudioMixer::pushMicPcm);
    connect(m_audioCapturer, &AudioCapturer::audioDataReady, this, [this](const QByteArray& pcm) {
        if (m_preview) {
            m_preview->updateMicLevel(calculateMicDbFs(pcm));
        }
    });
    connect(m_systemAudioCapturer, &SystemAudioCapturer::systemAudioDataReady, this,
            [this](const QByteArray& pcm, int sampleRate, int channels) {
                m_sender->onAudioDataReady(pcm);
                m_audioMixer->pushSystemPcm(pcm, sampleRate, channels);
                if (m_preview) {
                    m_preview->updateSystemLevel(calculateSystemDbFs(pcm, sampleRate, channels));
                }
            });
    connect(m_audioMixer, &AudioMixer::mixedAudioReady,
            this, &MeetingMainWindow::onMixedAudio,
            Qt::QueuedConnection);
    connect(m_toolbar, &ShareToolbar::localPlaybackToggled, this, [this](bool enabled) {
        if (enabled) {
            if (!m_localPlaybackWarningShown) {
                QMessageBox::warning(nullptr,
                                     QStringLiteral("本地回放警告"),
                                     QStringLiteral("外放扬声器会产生回声/啸叫，请佩戴耳机使用。"));
                m_localPlaybackWarningShown = true;
            }
            QAudioFormat format;
            format.setSampleRate(16000);
            format.setChannelCount(1);
            format.setSampleFormat(QAudioFormat::Int16);
            m_localPlayback->start(format);
            return;
        }

        m_localPlayback->stop();
    });

    static DebugTransport s_debugTransport;
    m_sender->setTransport(&s_debugTransport);
    m_sender->start();

    m_toolbar->hide();
}

MeetingMainWindow::~MeetingMainWindow()
{
    qDebug() << "[Meeting] dtor";
    stopSharing();
    m_sender->stop();
    if (m_toolbar) {
        m_toolbar->hide();
    }
    delete m_toolbar;
    m_toolbar = nullptr;
    if (m_annotationWindow) {
        m_annotationWindow->deleteLater();
        m_annotationWindow = nullptr;
    }
    if (m_preview) {
        m_preview->deleteLater();
        m_preview = nullptr;
    }
}

void MeetingMainWindow::startSharing(const ShareSelection& selection)
{
    qDebug() << "[Meeting] startSharing kind=" << static_cast<int>(selection.kind)
             << "screenIndex=" << selection.screenIndex
             << "hwnd=" << selection.hwnd
             << "includeSystemAudio=" << selection.includeSystemAudio
             << "fps=" << selection.fps;
    if (m_sharing || m_shareStartPending) {
        stopSharing();
    }

    m_currentSelection = selection;
    m_capturer->setWgcOptions(selection.includeCursor, selection.showBorder, 8);
    m_lastCaptureError.clear();
    m_shareStartPending = true;
    const int requestId = ++m_shareStartRequestId;

    hide();
    QApplication::processEvents();

    QTimer::singleShot(150, this, [this, selection, requestId]() {
        if (!m_shareStartPending || requestId != m_shareStartRequestId) {
            return;
        }

        ensurePreviewWindow();

        if (selection.kind == ShareSelection::Kind::Window) {
            m_capturer->startWindow(selection.hwnd, selection.fps);
            if (m_capturer->isRunning()) {
                m_windowFollowTimer->start(30);
            } else {
                m_windowFollowTimer->stop();
            }
        } else {
            const QList<QScreen*> screens = QGuiApplication::screens();
            if (selection.screenIndex >= 0 && selection.screenIndex < screens.size() && screens.at(selection.screenIndex)) {
                const QScreen* screen = screens.at(selection.screenIndex);
                const qreal dpr = screen->devicePixelRatio();
                m_capturer->setOutputSize(QSize(qRound(screen->size().width() * dpr),
                                                qRound(screen->size().height() * dpr)));
            }
            m_capturer->startScreen(selection.screenIndex, selection.fps);
            m_windowFollowTimer->stop();
        }

        if (!m_capturer->isRunning()) {
            m_shareStartPending = false;
            showNormal();
            raise();
            activateWindow();
            return;
        }

        m_audioCapturer->start();
        m_systemAudioCapturer->setEnabled(selection.includeSystemAudio);
        m_audioMixer->reset();
        if (m_preview) {
            m_preview->setDeviceLabels(QMediaDevices::defaultAudioInput().description(),
                                       QMediaDevices::defaultAudioOutput().description());
            m_preview->updateMicLevel(-90.0);
            m_preview->updateSystemLevel(-90.0);
        }

        applyAnnotationGeometry();

        m_toolbar->setPaused(false);
        m_toolbar->setAnnotationEnabled(false);
        m_toolbar->setMicMuted(false);
        m_toolbar->setSystemAudioEnabled(selection.includeSystemAudio);
        updateToolbarPosition();
        m_toolbar->show();
        m_toolbar->raise();

        updatePreviewPosition();
        m_preview->show();
        m_preview->raise();

        m_sharing = true;
        m_shareStartPending = false;
        m_endButton->setEnabled(true);
    });
}

void MeetingMainWindow::stopSharing()
{
    qDebug() << "[Meeting] stopSharing sharing=" << m_sharing;
    if (!m_sharing && !m_shareStartPending && !m_preview) {
        return;
    }

    ++m_shareStartRequestId;
    m_shareStartPending = false;
    m_windowFollowTimer->stop();
    m_previewRefreshTimer->stop();
    m_localPlayback->stop();
    m_audioMixer->reset();
    m_audioCapturer->stop();
    m_systemAudioCapturer->setEnabled(false);
    m_capturer->stop();

    if (m_annotationWindow) {
        disconnect(m_annotationWindow, nullptr, this, nullptr);
        m_annotationWindow->close();
        m_annotationWindow->deleteLater();
        m_annotationWindow = nullptr;
        m_toolbar->setAnnotationEnabled(false);
    }
    if (m_toolbar) {
        m_toolbar->setLocalPlaybackEnabled(false);
        m_toolbar->hide();
    }
    if (m_preview) {
        m_preview->hide();
        m_preview->deleteLater();
        m_preview = nullptr;
    }

    m_sharing = false;
    m_lastCaptureError.clear();
    m_lastRawFrame = QImage{};
    m_lastFrameBackend.clear();
    m_annotationLayerCache = QImage{};
    m_annotationLayerCacheSourceSize = QSize{};
    m_annotationLayerDirty = true;
    m_endButton->setEnabled(false);
    showNormal();
    raise();
    activateWindow();
}

void MeetingMainWindow::updateToolbarPosition()
{
    if (!m_toolbar) {
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (m_currentSelection.kind == ShareSelection::Kind::Screen) {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (m_currentSelection.screenIndex >= 0 && m_currentSelection.screenIndex < screens.size()) {
            screen = screens.at(m_currentSelection.screenIndex);
        }
    }
    if (!screen) {
        return;
    }

    const QRect g = screen->availableGeometry();
    m_toolbar->adjustSize();
    m_toolbar->move(g.left() + (g.width() - m_toolbar->width()) / 2, g.top() + 12);
}

void MeetingMainWindow::ensurePreviewWindow()
{
    if (m_preview) {
        return;
    }

    m_preview = new LocalPreviewWindow();
}

void MeetingMainWindow::updatePreviewPosition()
{
    if (!m_preview) {
        return;
    }

    QScreen* targetScreen = QGuiApplication::primaryScreen();
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (m_currentSelection.kind == ShareSelection::Kind::Screen) {
        if (m_currentSelection.screenIndex >= 0 && m_currentSelection.screenIndex < screens.size()) {
            if (screens.size() > 1) {
                for (int i = 0; i < screens.size(); ++i) {
                    if (i != m_currentSelection.screenIndex && screens.at(i)) {
                        targetScreen = screens.at(i);
                        break;
                    }
                }
            } else {
                targetScreen = screens.at(m_currentSelection.screenIndex);
            }
        }
    }
#ifdef Q_OS_WIN
    else {
        const HWND hwnd = reinterpret_cast<HWND>(m_currentSelection.hwnd);
        RECT rect{};
        if (hwnd && IsWindow(hwnd) && GetWindowRect(hwnd, &rect)) {
            targetScreen = QGuiApplication::screenAt(QPoint((rect.left + rect.right) / 2,
                                                            (rect.top + rect.bottom) / 2));
        }
    }
#endif
    if (!targetScreen) {
        return;
    }

    const QRect available = targetScreen->availableGeometry();
    const QSize previewSize = m_preview->size();
    m_preview->move(available.right() - previewSize.width() - 24,
                    available.bottom() - previewSize.height() - 24);
}

void MeetingMainWindow::handleCaptureError(const QString& error)
{
    if (m_preview) {
        m_preview->showError(error);
    }

    if (error.isEmpty() || error == m_lastCaptureError) {
        return;
    }

    m_lastCaptureError = error;
    QMessageBox::warning(nullptr, QStringLiteral("屏幕共享提示"), error);
}

void MeetingMainWindow::onFrameCaptured(const QImage& frame)
{
    m_lastCaptureError.clear();
    QImage processedFrame = frame;
    if (processedFrame.width() > 1920 || processedFrame.height() > 1080) {
        processedFrame = processedFrame.scaled(1920, 1080, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_lastRawFrame = processedFrame;

    const QImage composedFrame = composeFrameWithAnnotations(processedFrame);
    m_sender->onMainScreenFrameCaptured(composedFrame);
    if (m_preview) {
        m_preview->updateFrame(composedFrame);
    }

    if (!m_frameLogTimer.isValid() || m_frameLogTimer.elapsed() >= 1000) {
        m_frameLogTimer.restart();
        qDebug() << "[Frame] backend=" << m_lastFrameBackend
                 << "capture=" << frame.size()
                 << "processed=" << composedFrame.size();
    }
}

void MeetingMainWindow::refreshPreviewComposite()
{
    if (!m_preview || m_lastRawFrame.isNull()) {
        return;
    }
    m_previewRefreshTimer->start();
}

QImage MeetingMainWindow::composeFrameWithAnnotations(const QImage& frame)
{
    if (frame.isNull() || !m_annotationWindow || !m_annotationWindow->isVisible()) {
        return frame;
    }

    if (!m_annotationWindow->hasRenderableContent()) {
        return frame;
    }

    if (m_annotationLayerCacheSourceSize != frame.size()) {
        m_annotationLayerDirty = true;
    }

    if (m_annotationLayerDirty || m_annotationLayerCache.isNull()) {
        m_annotationLayerCache = m_annotationWindow->renderAnnotationsToImage(frame.size());
        m_annotationLayerCacheSourceSize = frame.size();
        m_annotationLayerDirty = false;
    }

    if (m_annotationLayerCache.isNull()) {
        return frame;
    }

    QImage composed = frame.copy();
    QPainter painter(&composed);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(0, 0, m_annotationLayerCache);
    return composed;
}

void MeetingMainWindow::onMixedAudio(const QByteArray& pcm)
{
    if (!m_mixedAudioLogTimer.isValid() || m_mixedAudioLogTimer.elapsed() >= 1000) {
        m_mixedAudioLogTimer.restart();
        qDebug() << "[Meeting] onMixedAudio bytes=" << pcm.size()
                 << "playbackRunning=" << (m_localPlayback && m_localPlayback->isRunning());
    }

    if (!m_localPlayback || !m_localPlayback->isRunning()) {
        return;
    }
    m_localPlayback->playData(pcm);
}

void MeetingMainWindow::applyAnnotationGeometry()
{
    if (!m_annotationWindow) {
        return;
    }

    if (m_currentSelection.kind == ShareSelection::Kind::Screen) {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (m_currentSelection.screenIndex < 0 || m_currentSelection.screenIndex >= screens.size()) {
            return;
        }
        QScreen* screen = screens.at(m_currentSelection.screenIndex);
        if (!screen) {
            return;
        }
        m_annotationWindow->setTargetGeometry(screen->geometry());
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_currentSelection.hwnd);
    if (!hwnd || !IsWindow(hwnd)) {
        m_annotationWindow->hide();
        return;
    }
    if (IsIconic(hwnd)) {
        m_annotationWindow->hide();
        return;
    }
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    const QRect geometry(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    m_annotationWindow->setTargetGeometry(geometry);
    if (!m_annotationWindow->isVisible()) {
        m_annotationWindow->show();
    }
    m_annotationWindow->raise();
    m_annotationWindow->activateWindow();
#endif
}
