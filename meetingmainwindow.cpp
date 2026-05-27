#include "meetingmainwindow.h"

#include "annotationoverlay.h"
#include "audiocapturer.h"
#include "screencapturer.h"
#include "sender.h"
#include "sharetoolbar.h"
#include "systemaudiocapturer.h"

#include <QGuiApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

MeetingMainWindow::MeetingMainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_capturer(new ScreenCapturer(this))
    , m_sender(new Sender(this))
    , m_audioCapturer(new AudioCapturer(this))
    , m_systemAudioCapturer(new SystemAudioCapturer(this))
    , m_overlay(new AnnotationOverlay())
    , m_toolbar(new ShareToolbar())
    , m_windowFollowTimer(new QTimer(this))
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

    m_overlay->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_overlay->hide();

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
        if (!m_overlay) {
            return;
        }
        if (enabled) {
            m_overlay->show();
            m_overlay->raise();
        } else {
            m_overlay->hide();
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

    connect(m_windowFollowTimer, &QTimer::timeout, this, &MeetingMainWindow::updateOverlayGeometry);

    connect(m_capturer, &ScreenCapturer::frameCaptured, m_sender, &Sender::onMainScreenFrameCaptured);
    connect(m_audioCapturer, &AudioCapturer::audioDataReady, m_sender, &Sender::onAudioDataReady);
    connect(m_systemAudioCapturer, &SystemAudioCapturer::systemAudioDataReady, this,
            [this](const QByteArray& pcm, int, int) {
                m_sender->onAudioDataReady(pcm);
            });

    connect(m_overlay, &AnnotationOverlay::strokePacketReady, m_sender, &Sender::onStrokePacketReady);
    connect(m_overlay, &AnnotationOverlay::textAnnotationCreated, m_sender, &Sender::onTextAnnotationCreated);

    static DebugTransport s_debugTransport;
    m_sender->setTransport(&s_debugTransport);
    m_sender->start();

    m_toolbar->hide();
}

MeetingMainWindow::~MeetingMainWindow()
{
    stopSharing();
    m_sender->stop();
    delete m_toolbar;
    m_toolbar = nullptr;
    delete m_overlay;
    m_overlay = nullptr;
}

void MeetingMainWindow::startSharing(const ShareSelection& selection)
{
    if (m_sharing) {
        stopSharing();
    }

    m_currentSelection = selection;
    m_capturer->setWgcOptions(selection.includeCursor, selection.showBorder, 8);

    if (selection.kind == ShareSelection::Kind::Window) {
        m_capturer->startWindow(selection.hwnd, selection.fps);
        m_windowFollowTimer->start(30);
    } else {
        m_capturer->startScreen(selection.screenIndex, selection.fps);
        m_windowFollowTimer->stop();
    }

    m_audioCapturer->start();
    m_systemAudioCapturer->setEnabled(selection.includeSystemAudio);

    updateOverlayGeometry();
    m_overlay->show();
    m_overlay->raise();

    m_toolbar->setPaused(false);
    m_toolbar->setAnnotationEnabled(true);
    m_toolbar->setMicMuted(false);
    m_toolbar->setSystemAudioEnabled(selection.includeSystemAudio);
    updateToolbarPosition();
    m_toolbar->show();
    m_toolbar->raise();

    m_sharing = true;
    m_endButton->setEnabled(true);
    hide();
}

void MeetingMainWindow::stopSharing()
{
    if (!m_sharing) {
        return;
    }

    m_windowFollowTimer->stop();
    m_capturer->stop();
    m_audioCapturer->stop();
    m_systemAudioCapturer->setEnabled(false);

    if (m_overlay) {
        m_overlay->hide();
        m_overlay->clearAll();
    }
    if (m_toolbar) {
        m_toolbar->hide();
    }

    m_sharing = false;
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

void MeetingMainWindow::updateOverlayGeometry()
{
    if (!m_overlay) {
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
        m_overlay->setGeometry(screen->geometry());
        m_overlay->showFullScreen();
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(m_currentSelection.hwnd);
    if (!hwnd || !IsWindow(hwnd)) {
        m_overlay->hide();
        return;
    }
    if (IsIconic(hwnd)) {
        m_overlay->hide();
        return;
    }
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    const QRect geometry(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    m_overlay->setGeometry(geometry);
    m_overlay->show();
    m_overlay->raise();
#endif
}
