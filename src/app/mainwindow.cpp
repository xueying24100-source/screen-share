#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../annotation/annotationwindow.h"
#include "../audio/audiocapturer.h"
#include "../audio/systemaudiocapturer.h"
#include "../audio/audiomixer.h"
#include "../media/sender.h"
#include "sharetoolbar.h"
#include "../audio/audioplayer.h"
#include "../media/cameramanager.h"
#include "../network/networktransport.h"
#include "../media/mediareceiver.h"
#include "../capture/screencapturer.h"
#include "../capture/sourceenumerator.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFontMetrics>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QStyle>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QWindow>
#include <QCursor>
#include <QDebug>
#include <QStringList>
#include <QPolygonF>
#include <QPen>
#include <QPainterPath>
#include <QLinearGradient>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    shareTimer = new QTimer(this);
    shareTimer->setInterval(160);   // 默认约 6fps；勾选“流畅模式”后会切到更高刷新率

    micCapturer = new AudioCapturer(this);
    systemAudioCapturer = new SystemAudioCapturer(this);
    audioMixer = new AudioMixer(this);
    localSender = new Sender(this);
    localAudioPlayer = new AudioPlayer(this);
    debugTransport = new DebugTransport();
    cameraManager = new CameraManager(this);
    cameraManager->setTargetFps(10);
    cameraManager->setMaxFrameSize(QSize(640, 360));
    networkTransport = new TcpPacketTransport(this);
    mediaReceiver = new MediaReceiver(this);
    screenCapturer = new ScreenCapturer(this);
    localSender->setTransport(debugTransport);

    connect(screenCapturer, &ScreenCapturer::frameCaptured,
            this, &MainWindow::onScreenCapturerFrame);
    connect(screenCapturer, &ScreenCapturer::captureError,
            this, &MainWindow::onScreenCapturerError);

    connect(micCapturer, &AudioCapturer::audioDataReady,
            audioMixer, &AudioMixer::pushMicPcm);
    connect(systemAudioCapturer, &SystemAudioCapturer::systemAudioDataReady,
            audioMixer, &AudioMixer::pushSystemPcm);
    connect(audioMixer, &AudioMixer::mixedAudioReady,
            localSender, &Sender::onAudioDataReady);
    connect(audioMixer, &AudioMixer::mixedAudioReady,
            localAudioPlayer, &AudioPlayer::playData);

    connect(micCapturer, &AudioCapturer::captureError, this, [this](const QString &error) {
        qWarning() << "[Mic]" << error;
        if (sharing) {
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，麦克风不可用");
        }
    });
    connect(systemAudioCapturer, &SystemAudioCapturer::captureError, this, [this](const QString &error) {
        qWarning() << "[SystemAudio]" << error;
        if (sharing) {
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，系统声音不可用");
        }
    });
    connect(localAudioPlayer, &AudioPlayer::playerError, this, [this](const QString &error) {
        qWarning() << "[AudioPlayer]" << error;
        if (sharing) {
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，本地回放不可用");
        }
    });

    connect(cameraManager, &CameraManager::frameReady,
            this, &MainWindow::onLocalCameraFrame);
    connect(cameraManager, &CameraManager::cameraStarted, this, [this](const QString &name) {
        cameraOn = true;
        if (btnCamera) {
            btnCamera->setText(QStringLiteral("关闭摄像头"));
        }
        ui->labelStatus->setText(QStringLiteral("状态：摄像头已打开：%1").arg(name));
        ensureSenderRunningForCamera();
    });
    connect(cameraManager, &CameraManager::cameraStopped, this, [this]() {
        cameraOn = false;
        if (btnCamera) {
            btnCamera->setText(QStringLiteral("打开摄像头"));
        }
        setSmallLabelPlaceholder(ui->labelSmall1, QStringLiteral("本机摄像头"));
        if (localSender) {
            localSender->setStreamEnabled(localSender->pipVideoStreamId(), false);
            localSender->sendCameraStopped();
        }
        if (!sharing && localSender) {
            localSender->stop();
            localSender->setTransport(nullptr);
        }
        ui->labelStatus->setText(QStringLiteral("状态：摄像头已关闭"));
    });
    connect(cameraManager, &CameraManager::cameraError, this, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("摄像头"), message);
    });

    connect(networkTransport, &TcpPacketTransport::packetReceived,
            mediaReceiver, &MediaReceiver::onPacketReceived);
    connect(networkTransport, &TcpPacketTransport::connectedChanged, this, [this](bool connected, const QString &message) {
        if (localSender) {
            localSender->setTransport(connected ? static_cast<INetworkTransport*>(networkTransport)
                                                : static_cast<INetworkTransport*>(debugTransport));
            if (connected && (sharing || cameraOn)) {
                localSender->start();
            }
        }
        ui->labelStatus->setText(QStringLiteral("状态：%1").arg(message));
    });
    connect(networkTransport, &TcpPacketTransport::transportError, this, [this](const QString &message) {
        qWarning() << "[Network]" << message;
        ui->labelStatus->setText(QStringLiteral("状态：%1").arg(message));
    });
    connect(mediaReceiver, &MediaReceiver::mainVideoFrameReceived,
            this, &MainWindow::onRemoteMainFrame);
    connect(mediaReceiver, &MediaReceiver::cameraFrameReceived,
            this, &MainWindow::onRemoteCameraFrame);
    connect(mediaReceiver, &MediaReceiver::cameraStoppedReceived, this, [this]() {
        setSmallLabelPlaceholder(ui->labelSmall2, QStringLiteral("远端摄像头"));
    });
    connect(mediaReceiver, &MediaReceiver::receiverMessage, this, [this](const QString &message) {
        qWarning() << "[Receiver]" << message;
    });

    ui->labelStatus->setText("状态：未共享");
    ui->labelMainScreen->setText("等待共享");
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);

    ui->labelSmall1->setAlignment(Qt::AlignCenter);
    ui->labelSmall2->setAlignment(Qt::AlignCenter);
    ui->labelSmall3->setAlignment(Qt::AlignCenter);
    ui->labelSmall4->setAlignment(Qt::AlignCenter);
    ui->labelSmall5->setAlignment(Qt::AlignCenter);
    setSmallLabelPlaceholder(ui->labelSmall1, QStringLiteral("本机摄像头"));
    setSmallLabelPlaceholder(ui->labelSmall2, QStringLiteral("远端摄像头"));

    setupMeetingControls();
    createSharePopup();

    connect(ui->btnShare, &QPushButton::clicked,
            this, &MainWindow::onShowSharePopup);

    connect(ui->btnEnd, &QPushButton::clicked,
            this, &MainWindow::endShare);

    ui->btnAnnotate->setEnabled(false);
    ui->btnAnnotate->setText("画笔");
    connect(ui->btnAnnotate, &QPushButton::clicked,
            this, &MainWindow::toggleAnnotationWindow);

    connect(shareTimer, &QTimer::timeout,
            this, &MainWindow::captureScreen);
}

MainWindow::~MainWindow()
{
    stopLocalMediaBackend();
    if (shareTimer) {
        shareTimer->stop();
    }
    if (annotationWindow) {
        annotationWindow->close();
    }
    if (shareToolbar) {
        shareToolbar->hide();
        delete shareToolbar;
        shareToolbar = nullptr;
    }
    if (localAudioPlayer) {
        localAudioPlayer->stop();
    }
    if (cameraManager) {
        cameraManager->stop();
    }
    if (networkTransport) {
        networkTransport->close();
    }
    if (localSender) {
        localSender->setTransport(nullptr);
    }
    delete debugTransport;
    debugTransport = nullptr;
    delete ui;
}

void MainWindow::createSharePopup()
{
    sharePopup = new QFrame(ui->centralwidget);
    sharePopup->setObjectName("sharePopup");
    sharePopup->setFixedSize(680, 620);
    sharePopup->hide();

    QVBoxLayout *mainLayout = new QVBoxLayout(sharePopup);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    QLabel *titleLabel = new QLabel("选择共享内容", sharePopup);
    titleLabel->setObjectName("sharePopupTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel *screenLabel = new QLabel("屏幕 / 白板", sharePopup);
    screenLabel->setObjectName("sharePopupGroupTitle");
    mainLayout->addWidget(screenLabel);

    shareGrid = new QGridLayout;
    shareGrid->setHorizontalSpacing(18);
    shareGrid->setVerticalSpacing(18);

    auto makeButton = [&](const QString &text, const QIcon &icon) {
        QToolButton *btn = new QToolButton(sharePopup);
        btn->setText(text);
        btn->setIcon(icon);
        btn->setIconSize(QSize(110, 62));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(150, 110);
        btn->setAutoRaise(false);
        return btn;
    };

    btnDesktop1 = makeButton("桌面1", makeScreenThumbnailIcon(0));
    btnDesktop2 = makeButton("桌面2", makeScreenThumbnailIcon(1));
    btnWhiteboard = makeButton("白板", style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    btnWhiteboard->setIconSize(QSize(54, 54));

    shareGrid->addWidget(btnDesktop1, 0, 0);
    shareGrid->addWidget(btnDesktop2, 0, 1);
    shareGrid->addWidget(btnWhiteboard, 0, 2);

    mainLayout->addLayout(shareGrid);

    windowGroupLabel = new QLabel("已打开的窗口 / 最小化窗口", sharePopup);
    windowGroupLabel->setObjectName("sharePopupGroupTitle");
    mainLayout->addWidget(windowGroupLabel);

    windowGrid = new QGridLayout;
    windowGrid->setHorizontalSpacing(18);
    windowGrid->setVerticalSpacing(18);
    mainLayout->addLayout(windowGrid);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setSpacing(24);

    checkShareAudio = new QCheckBox("共享音频", sharePopup);
    checkShowCursor = new QCheckBox("显示鼠标指针", sharePopup);
    checkShowCursor->setChecked(true);
    checkSmooth = new QCheckBox("流畅模式", sharePopup);

    bottomLayout->addWidget(checkShareAudio);
    bottomLayout->addWidget(checkShowCursor);
    bottomLayout->addWidget(checkSmooth);
    bottomLayout->addStretch();

    connect(checkSmooth, &QCheckBox::toggled, this, [this]() {
        updateShareTimerInterval();
        if (localSender) {
            const bool smooth = checkSmooth && checkSmooth->isChecked();
            localSender->setMaxFps(localSender->mainVideoStreamId(), smooth ? 10 : 6);
            localSender->setVideoQuality(localSender->mainVideoStreamId(), smooth ? 50 : 65);
            localSender->setVideoMaxSize(localSender->mainVideoStreamId(), smooth ? QSize(960, 540) : QSize(1280, 720));
        }
    });
    connect(checkShareAudio, &QCheckBox::toggled, this, [this](bool on) {
        if (sharing && systemAudioCapturer) {
            systemAudioCapturer->setEnabled(on);
        }
        if (sharing) {
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + optionSummary());
        }
    });

    mainLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    connect(btnDesktop1, &QToolButton::clicked,
            this, &MainWindow::onSelectDesktop1);

    connect(btnDesktop2, &QToolButton::clicked,
            this, &MainWindow::onSelectDesktop2);

    connect(btnWhiteboard, &QToolButton::clicked,
            this, &MainWindow::onSelectWhiteboard);

    refreshSharePopupOptions();
    centerSharePopup();
    sharePopup->raise();

    // 给浮层单独加样式
    sharePopup->setStyleSheet(R"(
        QFrame#sharePopup {
            background-color: white;
            border: 1px solid #dcdfe6;
            border-radius: 14px;
        }

        QLabel#sharePopupTitle {
            font-size: 26px;
            font-weight: bold;
            color: #1f2329;
        }

        QLabel#sharePopupGroupTitle {
            font-size: 16px;
            font-weight: bold;
            color: #4b5563;
        }

        QToolButton {
            background-color: #f7f8fa;
            border: 1px solid #dcdfe6;
            border-radius: 12px;
            font-size: 15px;
            font-weight: bold;
            color: #1f2329;
            padding: 8px;
        }

        QToolButton:hover {
            background-color: #e8f1ff;
            border: 1px solid #1677ff;
            color: #1677ff;
        }

        QToolButton:disabled {
            background-color: #f1f5f9;
            color: #94a3b8;
            border: 1px solid #e2e8f0;
        }

        QCheckBox {
            font-size: 15px;
            color: #333333;
        }
    )");
}

void MainWindow::refreshSharePopupOptions()
{
    if (!shareGrid || !windowGrid) {
        return;
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    if (btnDesktop1) {
        btnDesktop1->setIcon(makeScreenThumbnailIcon(0));
        btnDesktop1->setIconSize(QSize(110, 62));
        if (!screens.isEmpty() && screens.at(0)) {
            const QRect g = screens.at(0)->geometry();
            btnDesktop1->setToolTip(QStringLiteral("共享桌面1：%1×%2").arg(g.width()).arg(g.height()));
        }
    }
    if (btnDesktop2) {
        // 没有扩展屏时，直接隐藏“桌面2”。
        const bool hasSecondScreen = screens.size() >= 2;
        btnDesktop2->setVisible(hasSecondScreen);
        btnDesktop2->setIcon(makeScreenThumbnailIcon(1));
        btnDesktop2->setIconSize(QSize(110, 62));
        if (hasSecondScreen && screens.at(1)) {
            const QRect g = screens.at(1)->geometry();
            btnDesktop2->setToolTip(QStringLiteral("共享桌面2 / 扩展屏：%1×%2").arg(g.width()).arg(g.height()));
        } else {
            btnDesktop2->setToolTip(QStringLiteral("未检测到扩展屏"));
        }
    }

    clearWindowButtons();

    QList<WindowItem> windows = listOpenWindows();
    const int maxWindowButtons = 6;
    int count = 0;
    for (const WindowItem &item : windows) {
        if (count >= maxWindowButtons) {
            break;
        }

        QString text = shortWindowTitle(item.title);
        if (item.minimized) {
            text += "\n(最小化)";
        }

        QToolButton *btn = new QToolButton(sharePopup);
        btn->setText(text);
        btn->setToolTip(item.title);

        QImage thumbnail = ScreenCapturer::captureWindowOnce(item.handle, QSize(120, 68));
        if (!thumbnail.isNull()) {
            btn->setIcon(QPixmap::fromImage(thumbnail));
        } else {
            btn->setIcon(style()->standardIcon(item.minimized
                                                   ? QStyle::SP_TitleBarMinButton
                                                   : QStyle::SP_TitleBarNormalButton));
        }
        btn->setIconSize(QSize(120, 68));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(150, 110);

        const int row = count / 3;
        const int col = count % 3;
        windowGrid->addWidget(btn, row, col);
        windowSourceButtons.append(btn);

        const quintptr handle = item.handle;
        const QString title = item.title;
        connect(btn, &QToolButton::clicked, this, [this, handle, title]() {
            startShareWindow(handle, title);
        });

        ++count;
    }

    if (windowGroupLabel) {
#ifdef Q_OS_WIN
        windowGroupLabel->setText(count > 0
                                      ? QStringLiteral("已打开的窗口 / 最小化窗口")
                                      : QStringLiteral("未检测到可共享窗口"));
#else
        windowGroupLabel->setText(QStringLiteral("窗口列表：当前平台暂未实现自动枚举"));
#endif
    }
}

void MainWindow::clearWindowButtons()
{
    for (QToolButton *btn : windowSourceButtons) {
        if (!btn) {
            continue;
        }
        if (windowGrid) {
            windowGrid->removeWidget(btn);
        }
        btn->deleteLater();
    }
    windowSourceButtons.clear();
}

QList<MainWindow::WindowItem> MainWindow::listOpenWindows() const
{
    QList<WindowItem> items;
    const QList<WindowInfo> windows = SourceEnumerator::enumerateWindows();
    for (const auto& w : windows) {
        WindowItem item;
        item.handle = w.handle;
        item.title = w.title;
        item.minimized = w.minimized;
        items.append(item);
    }
    return items;
}

QString MainWindow::shortWindowTitle(const QString &title, int maxLen) const
{
    QString t = title.simplified();
    if (t.size() <= maxLen) {
        return t;
    }
    return t.left(maxLen) + QStringLiteral("...");
}

QIcon MainWindow::makeScreenThumbnailIcon(int screenIndex) const
{
    const QSize iconSize(110, 62);
    QPixmap canvas(iconSize);
    canvas.fill(Qt::transparent);

    QPixmap desktopPixmap;
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screenIndex >= 0 && screenIndex < screens.size() && screens.at(screenIndex)) {
        desktopPixmap = screens.at(screenIndex)->grabWindow(0);
    }

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(canvas.rect().adjusted(1, 1, -1, -1), 8, 8);
    painter.setClipPath(path);

    if (!desktopPixmap.isNull()) {
        QPixmap scaled = desktopPixmap.scaled(iconSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (scaled.width() - iconSize.width()) / 2;
        const int y = (scaled.height() - iconSize.height()) / 2;
        painter.drawPixmap(0, 0, scaled.copy(x, y, iconSize.width(), iconSize.height()));
    } else {
        QLinearGradient gradient(0, 0, 0, iconSize.height());
        gradient.setColorAt(0.0, QColor("#45d6e6"));
        gradient.setColorAt(1.0, QColor("#0f6fb2"));
        painter.fillRect(canvas.rect(), gradient);
        painter.setPen(QPen(QColor("#155e75"), 2));
        painter.drawLine(18, iconSize.height() - 10, iconSize.width() - 18, iconSize.height() - 10);
    }

    painter.setClipping(false);
    painter.setPen(QPen(QColor("#cbd5e1"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(canvas.rect().adjusted(0, 0, -1, -1), 8, 8);

    return QIcon(canvas);
}

void MainWindow::centerSharePopup()
{
    if (!sharePopup) return;

    int x = (ui->centralwidget->width() - sharePopup->width()) / 2;
    int y = (ui->centralwidget->height() - sharePopup->height()) / 2 - 20;

    if (x < 20) x = 20;
    if (y < 20) y = 20;

    sharePopup->move(x, y);
}

void MainWindow::showShareToolbar()
{
    if (!shareToolbar) {
        shareToolbar = new ShareToolbar(nullptr);

        connect(shareToolbar, &ShareToolbar::pauseToggled, this, [this](bool paused) {
            sharePaused = paused;
            if (shareTimer) {
                if (sharePaused) {
                    shareTimer->stop();
                } else if (sharing) {
                    shareTimer->start();
                    captureScreen();
                }
            }
            ui->labelStatus->setText(sharePaused
                                         ? QStringLiteral("状态：共享已暂停")
                                         : QStringLiteral("状态：正在共享 ") + currentShareSource + optionSummary());
        });

        connect(shareToolbar, &ShareToolbar::annotationToggled, this, [this](bool enabled) {
            setAnnotationEditingEnabled(enabled);
        });

        connect(shareToolbar, &ShareToolbar::micMuteToggled, this, [this](bool muted) {
            micMuted = muted;
            if (sharing && micCapturer) {
                if (micMuted) {
                    micCapturer->stop();
                } else {
                    micCapturer->start();
                }
            }
            ui->labelStatus->setText(QStringLiteral("状态：正在共享 ") + currentShareSource
                                     + (micMuted ? QStringLiteral("，麦克风已静音") : QStringLiteral("，麦克风已开启")));
        });

        connect(shareToolbar, &ShareToolbar::systemAudioToggled, this, [this](bool enabled) {
            if (checkShareAudio) {
                checkShareAudio->setChecked(enabled);
            }
            if (sharing && systemAudioCapturer) {
                systemAudioCapturer->setEnabled(enabled);
            }
            ui->labelStatus->setText(QStringLiteral("状态：正在共享 ") + currentShareSource
                                     + (enabled ? QStringLiteral("，共享声音已开启") : QStringLiteral("，共享声音已关闭")));
        });

        connect(shareToolbar, &ShareToolbar::localPlaybackToggled, this, [this](bool enabled) {
            if (!localAudioPlayer) {
                return;
            }
            if (enabled) {
                localAudioPlayer->start();
            } else {
                localAudioPlayer->stop();
            }
            if (sharing) {
                ui->labelStatus->setText(QStringLiteral("状态：正在共享 ") + currentShareSource
                                         + (enabled ? QStringLiteral("，本地回放已开启") : QStringLiteral("，本地回放已关闭")));
            }
        });

        connect(shareToolbar, &ShareToolbar::backRequested, this, [this]() {
            showNormal();
            raise();
            activateWindow();
            positionShareToolbar();
        });

        connect(shareToolbar, &ShareToolbar::stopRequested, this, [this]() {
            endShare();
        });
    }

    sharePaused = false;
    micMuted = false;
    shareToolbar->setPaused(false);
    shareToolbar->setAnnotationEnabled(annotationWindow && annotationWindow->isVisible());
    shareToolbar->setMicMuted(false);
    shareToolbar->setSystemAudioEnabled(checkShareAudio && checkShareAudio->isChecked());
    shareToolbar->setLocalPlaybackEnabled(localAudioPlayer && localAudioPlayer->isRunning());
    shareToolbar->show();
    shareToolbar->raise();
    positionShareToolbar();
}

void MainWindow::hideShareToolbar()
{
    if (shareToolbar) {
        shareToolbar->hide();
        shareToolbar->setPaused(false);
        shareToolbar->setAnnotationEnabled(false);
        shareToolbar->setLocalPlaybackEnabled(false);
    }
}

void MainWindow::positionShareToolbar()
{
    if (!shareToolbar || !shareToolbar->isVisible()) {
        return;
    }

    shareToolbar->adjustSize();
    QRect base = frameGeometry();
    QScreen *targetScreen = QGuiApplication::screenAt(base.center());
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    const QRect available = targetScreen ? targetScreen->availableGeometry() : QRect();

    int x = base.center().x() - shareToolbar->width() / 2;
    int y = base.top() + 8;

    if (available.isValid()) {
        x = qBound(available.left() + 8, x, available.right() - shareToolbar->width() - 8);
        y = qBound(available.top() + 8, y, available.bottom() - shareToolbar->height() - 8);
    }

    shareToolbar->move(x, y);
}

void MainWindow::setAnnotationEditingEnabled(bool enabled)
{
    if (!sharing) {
        return;
    }

    if (enabled) {
        showAnnotationWindow();
    } else if (annotationWindow && annotationWindow->isVisible()) {
        annotationWindow->hide();
        ui->btnAnnotate->setText("画笔");
        ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已关闭");
        if (currentShareType == ShareSourceType::Whiteboard) {
            updateWhiteboardPreview();
        } else {
            captureScreen();
        }
    }

    if (shareToolbar) {
        shareToolbar->setAnnotationEnabled(annotationWindow && annotationWindow->isVisible());
    }
}


QString MainWindow::optionSummary() const
{
    QStringList parts;
    if (checkShareAudio && checkShareAudio->isChecked()) {
        parts << QStringLiteral("共享音频");
    }
    if (checkShowCursor && checkShowCursor->isChecked() && currentShareType == ShareSourceType::Screen) {
        parts << QStringLiteral("显示鼠标");
    }
    if (checkSmooth && checkSmooth->isChecked()) {
        parts << QStringLiteral("流畅模式");
    }
    return parts.isEmpty() ? QString() : QStringLiteral("（%1）").arg(parts.join(QStringLiteral(" / ")));
}



void MainWindow::setupMeetingControls()
{
    // bottomBar 仍然是旧 UI 文件里的绝对定位控件。这里统一创建新增按钮，再交给 layoutMeetingControls() 自适应排布。
    if (!ui || !ui->bottomBar) {
        return;
    }

    btnHost = new QPushButton(QStringLiteral("开启监听"), ui->bottomBar);
    btnHost->setObjectName(QStringLiteral("btnHostMeeting"));
    btnHost->show();

    btnConnectLocal = new QPushButton(QStringLiteral("连接本机"), ui->bottomBar);
    btnConnectLocal->setObjectName(QStringLiteral("btnConnectLocal"));
    btnConnectLocal->show();

    btnCamera = new QPushButton(QStringLiteral("打开摄像头"), ui->bottomBar);
    btnCamera->setObjectName(QStringLiteral("btnCamera"));
    btnCamera->show();

    const QString extraButtonStyle = R"(
        QPushButton#btnHostMeeting, QPushButton#btnConnectLocal, QPushButton#btnCamera {
            border: none;
            border-radius: 12px;
            background-color: #e5e7eb;
            color: #111827;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#btnHostMeeting:hover, QPushButton#btnConnectLocal:hover, QPushButton#btnCamera:hover {
            background-color: #dbeafe;
            color: #1677ff;
        }
    )";
    ui->bottomBar->setStyleSheet(ui->bottomBar->styleSheet() + extraButtonStyle);

    layoutMeetingControls();

    connect(btnCamera, &QPushButton::clicked, this, &MainWindow::toggleCamera);
    connect(btnHost, &QPushButton::clicked, this, &MainWindow::startHostMeeting);
    connect(btnConnectLocal, &QPushButton::clicked, this, &MainWindow::connectLocalMeeting);
}

void MainWindow::layoutMeetingControls()
{
    if (!ui || !ui->bottomBar || !btnHost || !btnConnectLocal || !btnCamera) {
        return;
    }

    const int barW = qMax(ui->bottomBar->width(), 970);
    const int y = 28;
    const int h = 40;
    const int rightMargin = 22;
    int gap = 20;

    const QList<int> widths = {105, 105, 100, 90, 100, 115};
    int totalButtonsW = 0;
    for (int w : widths) {
        totalButtonsW += w;
    }
    int totalW = totalButtonsW + gap * (widths.size() - 1);

    // 默认状态栏保留一块空间；窗口较窄时自动压缩间距，避免按钮重叠。
    const int preferredStartX = 260;
    if (preferredStartX + totalW + rightMargin > barW) {
        gap = qMax(12, (barW - preferredStartX - rightMargin - totalButtonsW) / (widths.size() - 1));
        totalW = totalButtonsW + gap * (widths.size() - 1);
    }

    int x = qMax(preferredStartX, barW - totalW - rightMargin);
    const int statusW = qBound(220, x - 40, 420);
    ui->labelStatus->setGeometry(20, y, statusW, 32);

    QPushButton *buttons[] = {
        btnHost,
        btnConnectLocal,
        ui->btnShare,
        ui->btnAnnotate,
        ui->btnEnd,
        btnCamera
    };

    for (int i = 0; i < widths.size(); ++i) {
        buttons[i]->setGeometry(x, y, widths[i], h);
        x += widths[i] + gap;
    }
}

INetworkTransport *MainWindow::activeTransport() const
{
    if (networkTransport && networkTransport->isConnected()) {
        return networkTransport;
    }
    return debugTransport;
}

void MainWindow::ensureSenderRunningForCamera()
{
    if (!localSender) {
        return;
    }
    localSender->setTransport(activeTransport());
    localSender->setStreamEnabled(localSender->pipVideoStreamId(), true);
    localSender->setMaxFps(localSender->pipVideoStreamId(), 10);
    localSender->setVideoQuality(localSender->pipVideoStreamId(), 60);
    localSender->setVideoMaxSize(localSender->pipVideoStreamId(), QSize(480, 270));
    if (!localSender->isRunning()) {
        localSender->start();
    }
}

void MainWindow::updateVideoLabel(QLabel *label, const QImage &image, const QString &fallbackText)
{
    if (!label) {
        return;
    }
    if (image.isNull()) {
        setSmallLabelPlaceholder(label, fallbackText);
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image).scaled(
        label->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation);
    label->setText(QString());
    label->setPixmap(pixmap);
    label->setAlignment(Qt::AlignCenter);
}

void MainWindow::setSmallLabelPlaceholder(QLabel *label, const QString &text)
{
    if (!label) {
        return;
    }
    label->clear();
    label->setPixmap(QPixmap());
    label->setText(text);
    label->setAlignment(Qt::AlignCenter);
}

void MainWindow::toggleCamera()
{
    if (!cameraManager) {
        return;
    }

    if (cameraOn || cameraManager->isRunning()) {
        cameraManager->stop();
        return;
    }

    const QStringList names = cameraManager->availableCameraNames();
    if (names.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("摄像头"), QStringLiteral("未检测到可用摄像头。"));
        return;
    }

    int index = 0;
    if (names.size() > 1) {
        bool ok = false;
        const QString selected = QInputDialog::getItem(this,
                                                       QStringLiteral("选择摄像头"),
                                                       QStringLiteral("摄像头设备："),
                                                       names,
                                                       0,
                                                       false,
                                                       &ok);
        if (!ok) {
            return;
        }
        index = names.indexOf(selected);
        if (index < 0) {
            index = 0;
        }
    }

    cameraManager->startCameraByIndex(index);
}

void MainWindow::startHostMeeting()
{
    if (!networkTransport) {
        return;
    }
    constexpr quint16 kDefaultPort = 9000;
    if (networkTransport->listen(kDefaultPort)) {
        if (localSender) {
            localSender->setTransport(activeTransport());
        }
    }
}

void MainWindow::connectLocalMeeting()
{
    if (!networkTransport) {
        return;
    }
    constexpr quint16 kDefaultPort = 9000;
    networkTransport->connectToPeer(QStringLiteral("127.0.0.1"), kDefaultPort);
}

void MainWindow::onLocalCameraFrame(const QImage &image)
{
    updateVideoLabel(ui->labelSmall1, image, QStringLiteral("本机摄像头"));
    if (localSender && localSender->isRunning()) {
        localSender->onPipFrameCaptured(image);
    }
}

void MainWindow::onRemoteMainFrame(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    // 自己正在共享时，大窗口优先显示本地共享；自己没共享时，大窗口显示远端共享画面。
    if (sharing) {
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image).scaled(
        ui->labelMainScreen->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation);
    ui->labelMainScreen->setStyleSheet("");
    ui->labelMainScreen->setPixmap(pixmap);
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);
}

void MainWindow::onRemoteCameraFrame(const QImage &image)
{
    updateVideoLabel(ui->labelSmall2, image, QStringLiteral("远端摄像头"));
}

void MainWindow::onScreenCapturerFrame(const QImage &frame)
{
    if (!sharing || sharePaused) {
        return;
    }

    updateAnnotationWindowGeometry();

    if (currentShareType == ShareSourceType::Whiteboard) {
        updateWhiteboardPreview();
        return;
    }

    if (frame.isNull()) {
        ui->labelMainScreen->clear();
        ui->labelMainScreen->setText("暂时无法获取共享画面");
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(frame);
    pixmap = composeCursorOnPixmap(pixmap);
    pixmap = composeAnnotationOnPixmap(pixmap);

    ui->labelMainScreen->setStyleSheet("");
    updatePreviewWithPixmap(pixmap);
}

void MainWindow::onScreenCapturerError(const QString &msg)
{
    qWarning() << "[ScreenCapturer]" << msg;
    if (msg.contains("closed", Qt::CaseInsensitive)) {
        ui->labelStatus->setText("状态：共享窗口已关闭");
    } else if (msg.contains("最小化") || msg.contains("minimized", Qt::CaseInsensitive)) {
        ui->labelStatus->setText("状态：共享窗口已最小化");
    } else {
        ui->labelStatus->setText("状态：" + msg);
    }
}

void MainWindow::updateShareTimerInterval()
{
    if (!shareTimer) {
        return;
    }
    // 流畅模式：约 10fps，并降低发送分辨率；普通模式：约 6fps，CPU 压力更低。
    const bool smooth = checkSmooth && checkSmooth->isChecked();
    shareTimer->setInterval(smooth ? 100 : 160);
}

void MainWindow::startLocalMediaBackend()
{
    updateShareTimerInterval();

    if (audioMixer) {
        audioMixer->reset();
    }
    if (localAudioPlayer) {
        localAudioPlayer->stop();
    }
    if (localSender) {
        localSender->setTransport(activeTransport());
        const bool smooth = checkSmooth && checkSmooth->isChecked();
        localSender->setMaxFps(localSender->mainVideoStreamId(), smooth ? 10 : 6);
        localSender->setVideoQuality(localSender->mainVideoStreamId(), smooth ? 50 : 65);
        localSender->setVideoMaxSize(localSender->mainVideoStreamId(), smooth ? QSize(960, 540) : QSize(1280, 720));
        localSender->start();
    }

    // 会议中默认开启麦克风采集；“共享音频”只控制系统声音 loopback。
    if (micCapturer) {
        micCapturer->start();
    }
    if (systemAudioCapturer) {
        systemAudioCapturer->setEnabled(checkShareAudio && checkShareAudio->isChecked());
    }
}

void MainWindow::stopLocalMediaBackend()
{
    if (systemAudioCapturer) {
        systemAudioCapturer->setEnabled(false);
    }
    if (micCapturer) {
        micCapturer->stop();
    }
    if (audioMixer) {
        audioMixer->reset();
    }
    if (localAudioPlayer) {
        localAudioPlayer->stop();
    }
    if (localSender) {
        if (!cameraOn) {
            localSender->stop();
            localSender->setTransport(nullptr);
        }
    }
}

void MainWindow::sendFrameToLocalSender(const QPixmap &pixmap)
{
    if (!localSender || !localSender->isRunning() || pixmap.isNull()) {
        return;
    }
    localSender->onMainScreenFrameCaptured(pixmap.toImage());
}

void MainWindow::connectAnnotationToSender(AnnotationWindow *window)
{
    if (!window || !localSender) {
        return;
    }
    connect(window, &AnnotationWindow::strokePacketReady,
            localSender, &Sender::onStrokePacketReady, Qt::UniqueConnection);
    connect(window, &AnnotationWindow::textAnnotationCreated,
            localSender, &Sender::onTextAnnotationCreated, Qt::UniqueConnection);
}

QPixmap MainWindow::composeCursorOnPixmap(const QPixmap &basePixmap) const
{
    if (basePixmap.isNull() || !checkShowCursor || !checkShowCursor->isChecked()) {
        return basePixmap;
    }
    if (currentShareType != ShareSourceType::Screen) {
        return basePixmap;
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    if (currentScreenIndex < 0 || currentScreenIndex >= screens.size() || !screens.at(currentScreenIndex)) {
        return basePixmap;
    }

    const QRect screenRect = screens.at(currentScreenIndex)->geometry();
    const QPoint globalCursor = QCursor::pos();
    if (!screenRect.contains(globalCursor)) {
        return basePixmap;
    }

    const QPoint localPos = globalCursor - screenRect.topLeft();
    const qreal sx = static_cast<qreal>(basePixmap.width()) / qMax(1, screenRect.width());
    const qreal sy = static_cast<qreal>(basePixmap.height()) / qMax(1, screenRect.height());
    const QPoint drawPos(qRound(localPos.x() * sx), qRound(localPos.y() * sy));

    QPixmap composed = basePixmap.copy();
    QPainter painter(&composed);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 简单绘制鼠标指针，避免依赖平台 cursor bitmap；第二阶段也方便作为普通视频内容发送。
    QPolygonF cursorShape;
    cursorShape << QPointF(0, 0)
                << QPointF(0, 24)
                << QPointF(6, 18)
                << QPointF(10, 30)
                << QPointF(14, 28)
                << QPointF(10, 17)
                << QPointF(20, 17);
    painter.translate(drawPos);
    painter.setBrush(Qt::white);
    painter.setPen(QPen(Qt::black, 1.5));
    painter.drawPolygon(cursorShape);
    return composed;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    layoutMeetingControls();
    centerSharePopup();
    positionShareToolbar();
    updateAnnotationWindowGeometry();
    if (currentShareType == ShareSourceType::Whiteboard) {
        updateWhiteboardPreview();
    } else if (sharing) {
        captureScreen();
    }
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    positionShareToolbar();
    updateAnnotationWindowGeometry();
    if (sharing && currentShareType != ShareSourceType::Whiteboard) {
        captureScreen();
    }
}

void MainWindow::onShowSharePopup()
{
    if (sharing) {
        return;
    }

    refreshSharePopupOptions();
    centerSharePopup();
    sharePopup->show();
    sharePopup->raise();
}

void MainWindow::startShareByName(const QString &sourceName)
{
    // 兼容旧逻辑。现在桌面 / 白板 / 窗口会分别走更明确的函数。
    if (sourceName == QStringLiteral("白板")) {
        startShareWhiteboard();
    } else if (sourceName == QStringLiteral("桌面2")) {
        startShareScreen(1);
    } else {
        startShareScreen(0);
    }
}

void MainWindow::startShareScreen(int screenIndex)
{
    if (sharePopup) {
        sharePopup->hide();
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.size()) {
        QMessageBox::warning(this, "提示", "未检测到该屏幕，请确认是否已经连接扩展屏。 ");
        return;
    }

    if (shareTimer) {
        shareTimer->stop();
    }
    if (screenCapturer) {
        screenCapturer->stop();
    }

    sharing = true;
    currentShareType = ShareSourceType::Screen;
    currentScreenIndex = screenIndex;
    currentWindowHandle = 0;
    currentShareSource = QStringLiteral("桌面%1").arg(screenIndex + 1);

    ui->labelMainScreen->setStyleSheet("");
    startLocalMediaBackend();
    ui->labelStatus->setText("状态：正在共享 " + currentShareSource + optionSummary());
    ui->btnShare->setText("共享中");
    ui->btnAnnotate->setEnabled(true);
    ui->btnAnnotate->setText("画笔");
    showShareToolbar();

    if (screenCapturer) {
        screenCapturer->startScreen(screenIndex, checkSmooth && checkSmooth->isChecked() ? 30 : 10);
    }
}

void MainWindow::startShareWhiteboard()
{
    if (sharePopup) {
        sharePopup->hide();
    }

    if (shareTimer) {
        shareTimer->stop();
    }

    sharing = true;
    currentShareType = ShareSourceType::Whiteboard;
    currentScreenIndex = 0;
    currentWindowHandle = 0;
    currentShareSource = QStringLiteral("白板");

    startLocalMediaBackend();
    ui->labelStatus->setText("状态：正在共享白板" + optionSummary());
    ui->btnShare->setText("共享中");
    ui->btnAnnotate->setEnabled(true);
    ui->btnAnnotate->setText("画笔");
    showShareToolbar();

    showWhiteboardPreview();
    if (shareTimer) {
        shareTimer->start();
    }
}

void MainWindow::startShareWindow(quintptr windowHandle, const QString &windowTitle)
{
    if (windowHandle == 0) {
        QMessageBox::warning(this, "提示", "没有获取到有效窗口。 ");
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    if (!IsWindow(hwnd)) {
        QMessageBox::warning(this, "提示", "该窗口已经关闭，请重新打开共享列表。 ");
        return;
    }

    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }
#endif

    if (sharePopup) {
        sharePopup->hide();
    }

    if (shareTimer) {
        shareTimer->stop();
    }
    if (screenCapturer) {
        screenCapturer->stop();
    }

    sharing = true;
    currentShareType = ShareSourceType::Window;
    currentScreenIndex = 0;
    currentWindowHandle = windowHandle;
    currentShareSource = QStringLiteral("窗口：") + shortWindowTitle(windowTitle, 20);

    ui->labelMainScreen->setStyleSheet("");
    startLocalMediaBackend();
    ui->labelStatus->setText("状态：正在共享 " + currentShareSource + optionSummary());
    ui->btnShare->setText("共享中");
    ui->btnAnnotate->setEnabled(true);
    ui->btnAnnotate->setText("画笔");
    showShareToolbar();

    if (screenCapturer) {
        screenCapturer->setWgcOptions(checkShowCursor && checkShowCursor->isChecked(), true, 0);
        screenCapturer->startWindow(windowHandle, checkSmooth && checkSmooth->isChecked() ? 30 : 10);
    }
}

void MainWindow::onSelectDesktop1()
{
    startShareScreen(0);
}

void MainWindow::onSelectDesktop2()
{
    startShareScreen(1);
}

void MainWindow::onSelectWhiteboard()
{
    startShareWhiteboard();
}

void MainWindow::onSelectCurrentWindow()
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    wchar_t titleBuffer[512] = {0};
    GetWindowTextW(hwnd, titleBuffer, 511);
    const QString title = QString::fromWCharArray(titleBuffer).trimmed();
    if (!title.isEmpty()) {
        startShareWindow(reinterpret_cast<quintptr>(hwnd), title);
        return;
    }
#endif
    QMessageBox::information(this, "提示", "当前活动窗口不可共享，请从下方窗口列表中选择。 ");
}

void MainWindow::showAnnotationWindow()
{
    if (!sharing) {
        QMessageBox::warning(this, "提示", "请先开始共享，再打开画笔标注。");
        return;
    }

    if (annotationWindow) {
        updateAnnotationWindowGeometry();
        annotationWindow->show();
        annotationWindow->raise();
        annotationWindow->activateWindow();
        ui->btnAnnotate->setText("关闭画笔");
        ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已打开");
        return;
    }

    // 画笔层要和“被共享的内容区域”保持一致：
    // 白板 -> 大屏白板区域；桌面 -> 对应屏幕；窗口 -> 被共享窗口。
    // 这样笔迹可以按同一坐标系合成进共享画面。
    const QRect targetRect = annotationTargetGlobalRect();

    annotationWindow = targetRect.isValid()
                           ? new AnnotationWindow(nullptr, targetRect)
                           : new AnnotationWindow();
    // 不直接销毁：关闭画笔只是停止继续编辑，已经画出的标注仍会合成到共享画面中。
    // 真正删除标注用工具栏的“清空”，结束共享时再统一销毁。
    annotationWindow->setAttribute(Qt::WA_DeleteOnClose, false);

    connect(annotationWindow, &AnnotationWindow::contentChanged, this, [this]() {
        // 不在每个鼠标移动点都立即抓屏。抓屏/合成由 shareTimer 控制，
        // 这样可以避免画笔移动时触发大量 PrintWindow/grabWindow，明显减少卡顿。
        if (currentShareType == ShareSourceType::Whiteboard) {
            updateWhiteboardPreview();
        }
    });

    connectAnnotationToSender(annotationWindow);

    connect(annotationWindow, &AnnotationWindow::closed, this, [this]() {
        if (annotationWindow) {
            annotationWindow->hide();
        }
        if (currentShareType == ShareSourceType::Whiteboard) {
            updateWhiteboardPreview();
        } else {
            captureScreen();
        }
        if (sharing) {
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已关闭");
            ui->btnAnnotate->setText("画笔");
            if (shareToolbar) {
                shareToolbar->setAnnotationEnabled(false);
            }
        } else {
            ui->labelStatus->setText("状态：未共享");
        }
    });

    connect(annotationWindow, &QObject::destroyed, this, [this]() {
        annotationWindow = nullptr;
        if (sharing) {
            ui->btnAnnotate->setText("画笔");
        }
    });

    ui->btnAnnotate->setText("关闭画笔");
    ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已打开");
    if (shareToolbar) {
        shareToolbar->setAnnotationEnabled(true);
    }
    if (currentShareType == ShareSourceType::Whiteboard) {
        updateWhiteboardPreview();
    }
}

void MainWindow::toggleAnnotationWindow()
{
    if (!sharing) {
        QMessageBox::warning(this, "提示", "请先开始共享，再打开画笔标注。 ");
        return;
    }

    if (annotationWindow) {
        // 所有共享模式都采用 hide/show：
        // 关闭画笔 = 停止编辑；已经画出的笔迹仍然参与共享画面的合成。
        if (annotationWindow->isVisible()) {
            annotationWindow->hide();
            ui->btnAnnotate->setText("画笔");
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已关闭");
            if (shareToolbar) {
                shareToolbar->setAnnotationEnabled(false);
            }
            if (currentShareType == ShareSourceType::Whiteboard) {
                updateWhiteboardPreview();
            } else {
                captureScreen();
            }
        } else {
            updateAnnotationWindowGeometry();
            annotationWindow->show();
            annotationWindow->raise();
            annotationWindow->activateWindow();
            ui->btnAnnotate->setText("关闭画笔");
            ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已打开");
            if (shareToolbar) {
                shareToolbar->setAnnotationEnabled(true);
            }
        }
        return;
    }

    showAnnotationWindow();
}

void MainWindow::updatePreviewWithPixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        return;
    }

    sendFrameToLocalSender(pixmap);

    QPixmap mainPixmap = pixmap.scaled(
        ui->labelMainScreen->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation
        );

    ui->labelMainScreen->setPixmap(mainPixmap);

    // 五个小窗口现在表示参会用户/摄像头，不再重复显示共享屏幕。
}

void MainWindow::showWhiteboardPreview()
{
    ui->labelMainScreen->setStyleSheet(R"(
        QLabel#labelMainScreen {
            background-color: #ffffff;
            border: 2px solid #dcdfe6;
            border-radius: 14px;
            color: #94a3b8;
            font-size: 24px;
            font-weight: bold;
        }
    )");
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);

    updateWhiteboardPreview();

    // 白板共享只占用大窗口，小窗口继续留给本机/远端摄像头。
    if (!cameraOn) {
        setSmallLabelPlaceholder(ui->labelSmall1, QStringLiteral("本机摄像头"));
    }
}

QRect MainWindow::whiteboardGlobalRect() const
{
    if (!ui || !ui->labelMainScreen) {
        return QRect();
    }
    return QRect(ui->labelMainScreen->mapToGlobal(QPoint(0, 0)), ui->labelMainScreen->size());
}

QRect MainWindow::windowGlobalRect(quintptr windowHandle) const
{
    if (windowHandle == 0) {
        return QRect();
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    if (!IsWindow(hwnd)) {
        return QRect();
    }
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return QRect();
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return QRect();
    }
    return QRect(rect.left, rect.top, width, height);
#else
    Q_UNUSED(windowHandle);
    return QRect();
#endif
}

QRect MainWindow::annotationTargetGlobalRect() const
{
    switch (currentShareType) {
    case ShareSourceType::Whiteboard:
        return whiteboardGlobalRect();
    case ShareSourceType::Screen: {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (currentScreenIndex >= 0 && currentScreenIndex < screens.size() && screens.at(currentScreenIndex)) {
            return screens.at(currentScreenIndex)->geometry();
        }
        return QRect();
    }
    case ShareSourceType::Window:
        return windowGlobalRect(currentWindowHandle);
    case ShareSourceType::None:
        return QRect();
    }
    return QRect();
}

void MainWindow::updateAnnotationWindowGeometry()
{
    if (!annotationWindow) {
        return;
    }

    const QRect area = annotationTargetGlobalRect();
    if (area.isValid()) {
        annotationWindow->setAnnotationGeometry(area);
    }
}

QPixmap MainWindow::composeAnnotationOnPixmap(const QPixmap &basePixmap) const
{
    if (basePixmap.isNull() || !annotationWindow || !annotationWindow->hasAnnotationContent()) {
        return basePixmap;
    }

    QPixmap ink = annotationWindow->annotationPixmap();
    if (ink.isNull()) {
        return basePixmap;
    }

    QPixmap composed = basePixmap.copy();
    if (ink.size() != composed.size()) {
        ink = ink.scaled(composed.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QPainter painter(&composed);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.drawPixmap(0, 0, ink);
    return composed;
}

QPixmap MainWindow::buildWhiteboardPixmap() const
{
    const QSize canvasSize = ui->labelMainScreen->size();
    if (!canvasSize.isValid()) {
        return QPixmap();
    }

    QPixmap canvas(canvasSize);
    canvas.fill(Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const bool hasInk = annotationWindow && annotationWindow->hasAnnotationContent();
    if (hasInk) {
        QPixmap ink = annotationWindow->annotationPixmap();
        if (!ink.isNull()) {
            if (ink.size() != canvasSize) {
                ink = ink.scaled(canvasSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            painter.drawPixmap(0, 0, ink);
        }
    } else {
        painter.setPen(QColor("#94a3b8"));
        QFont font = painter.font();
        font.setPointSize(18);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(canvas.rect(), Qt::AlignCenter,
                         QStringLiteral("白板共享中\n点击底部“画笔”进行标注"));
    }

    return canvas;
}

void MainWindow::updateWhiteboardPreview()
{
    if (currentShareType != ShareSourceType::Whiteboard) {
        return;
    }

    const QPixmap whiteboard = buildWhiteboardPixmap();
    if (whiteboard.isNull()) {
        return;
    }

    // 画笔窗口显示时，大屏区域由透明画笔层自己绘制；下面的 QLabel 保持白底，
    // 避免同一条线在 QLabel 和 overlay 上各画一次导致颜色变深。
    // 小窗口仍然更新，模拟“接收端已经收到白板笔画”。
    if (annotationWindow && annotationWindow->isVisible()) {
        sendFrameToLocalSender(whiteboard);
        ui->labelMainScreen->clear();
        ui->labelMainScreen->setText(QString());
        ui->labelMainScreen->setAlignment(Qt::AlignCenter);

        return;
    }

    updatePreviewWithPixmap(whiteboard);
}


bool MainWindow::pixmapLooksMostlyBlack(const QPixmap &pixmap) const
{
    if (pixmap.isNull()) {
        return true;
    }

    QImage img = pixmap.toImage().convertToFormat(QImage::Format_RGB32);
    if (img.isNull()) {
        return true;
    }

    const int stepX = qMax(1, img.width() / 80);
    const int stepY = qMax(1, img.height() / 80);
    int total = 0;
    int dark = 0;

    for (int y = 0; y < img.height(); y += stepY) {
        const QRgb *line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x += stepX) {
            const QRgb c = line[x];
            const int brightness = qRed(c) + qGreen(c) + qBlue(c);
            if (brightness < 30) {
                ++dark;
            }
            ++total;
        }
    }

    return total > 0 && dark > total * 92 / 100;
}

QPixmap MainWindow::captureWindowPixmap(quintptr windowHandle) const
{
    if (windowHandle == 0) {
        return QPixmap();
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    if (!IsWindow(hwnd)) {
        return QPixmap();
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return QPixmap();
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return QPixmap();
    }

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::black);

    HDC screenDc = GetDC(nullptr);
    HDC memDc = screenDc ? CreateCompatibleDC(screenDc) : nullptr;
    HBITMAP bitmap = (screenDc && memDc) ? CreateCompatibleBitmap(screenDc, width, height) : nullptr;
    HGDIOBJ oldBitmap = nullptr;

    bool captured = false;
    if (bitmap) {
        oldBitmap = SelectObject(memDc, bitmap);

        // PrintWindow 比 QScreen::grabWindow(hwnd) 更适合抓外部应用窗口，
        // 对 Qt Creator、浏览器、飞书这类窗口更不容易出现纯黑预览。
        captured = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);

        // PrintWindow 某些窗口可能失败，退回到窗口 DC 拷贝。
        if (!captured) {
            HDC windowDc = GetWindowDC(hwnd);
            if (windowDc) {
                captured = BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY | CAPTUREBLT);
                ReleaseDC(hwnd, windowDc);
            }
        }

        if (captured) {
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = width;
            bmi.bmiHeader.biHeight = -height; // top-down，避免图像上下颠倒
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            const int ok = GetDIBits(memDc, bitmap, 0, height, image.bits(), &bmi, DIB_RGB_COLORS);
            captured = ok != 0;
        }
    }

    if (oldBitmap) {
        SelectObject(memDc, oldBitmap);
    }
    if (bitmap) {
        DeleteObject(bitmap);
    }
    if (memDc) {
        DeleteDC(memDc);
    }
    if (screenDc) {
        ReleaseDC(nullptr, screenDc);
    }

    QPixmap result = captured ? QPixmap::fromImage(image) : QPixmap();

    // 如果 GDI 抓出来还是几乎全黑，就退回到“屏幕矩形裁剪”。
    // 这个方法要求目标窗口可见；所以最小化窗口会在 startShareWindow() 里先恢复。
    if (pixmapLooksMostlyBlack(result)) {
        QPoint center((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
        QScreen *targetScreen = QGuiApplication::screenAt(center);
        if (!targetScreen) {
            targetScreen = QGuiApplication::primaryScreen();
        }
        if (targetScreen) {
            QPixmap cropped = targetScreen->grabWindow(0, rect.left, rect.top, width, height);
            if (!pixmapLooksMostlyBlack(cropped)) {
                result = cropped;
            }
        }
    }

    return result;
#else
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen ? screen->grabWindow(static_cast<WId>(windowHandle)) : QPixmap();
#endif
}

void MainWindow::captureScreen()
{
    if (!sharing || sharePaused) {
        return;
    }

    updateAnnotationWindowGeometry();

    if (currentShareType == ShareSourceType::Whiteboard) {
        updateWhiteboardPreview();
    }
}

void MainWindow::endShare()
{
    stopLocalMediaBackend();

    if (shareTimer) {
        shareTimer->stop();
    }

    if (screenCapturer) {
        screenCapturer->stop();
    }

    if (sharePopup) {
        sharePopup->hide();
    }

    hideShareToolbar();

    if (annotationWindow) {
        annotationWindow->hide();
        annotationWindow->deleteLater();
        annotationWindow = nullptr;
    }

    sharing = false;
    sharePaused = false;
    micMuted = false;
    currentShareSource.clear();
    currentShareType = ShareSourceType::None;
    currentScreenIndex = 0;
    currentWindowHandle = 0;

    ui->labelStatus->setText("状态：未共享");
    ui->btnShare->setText("开始共享");
    ui->btnAnnotate->setEnabled(false);
    ui->btnAnnotate->setText("画笔");

    ui->labelMainScreen->clear();
    ui->labelMainScreen->setStyleSheet("");
    ui->labelMainScreen->setText("等待共享");
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);

    if (!cameraOn) {
        setSmallLabelPlaceholder(ui->labelSmall1, QStringLiteral("本机摄像头"));
    }
    setSmallLabelPlaceholder(ui->labelSmall3, QStringLiteral("用户3"));
    setSmallLabelPlaceholder(ui->labelSmall4, QStringLiteral("用户4"));
    setSmallLabelPlaceholder(ui->labelSmall5, QStringLiteral("用户5"));
}
