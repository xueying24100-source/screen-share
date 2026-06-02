#include "mainwindow_impl_includes.h"

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
    localSender->setTransport(debugTransport);

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
        setParticipantPlaceholder(localParticipantId, QStringLiteral("我\n摄像头未打开"));
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
        QString text = message.trimmed();
        if (text.isEmpty()) {
            text = QStringLiteral("摄像头打开失败：该设备可能已被其他程序占用，或当前没有权限访问摄像头。");
        }
        QMessageBox::warning(this,
                             QStringLiteral("摄像头"),
                             QStringLiteral("%1\n\n建议：\n1. 关闭其他窗口中正在使用的摄像头；\n2. 本机多开测试时，同一个物理摄像头通常只能被一个程序占用；\n3. 如有多个摄像头，可以选择其他摄像头设备。").arg(text));
    });

    connect(networkTransport, &TcpPacketTransport::packetReceivedFromPeer,
            this, &MainWindow::onMeetingPacketReceived);
    connect(networkTransport, &TcpPacketTransport::localIdentityAssigned,
            this, &MainWindow::onLocalIdentityAssigned);
    connect(networkTransport, &TcpPacketTransport::participantJoined,
            this, &MainWindow::onParticipantJoined);
    connect(networkTransport, &TcpPacketTransport::participantLeft,
            this, &MainWindow::onParticipantLeft);
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
    connect(mediaReceiver, &MediaReceiver::receiverMessage, this, [this](const QString &message) {
        qWarning() << "[Receiver]" << message;
    });

    ui->labelStatus->setText("状态：未共享");
    ui->labelStatus->setWordWrap(true);
    ui->labelStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->labelMainScreen->setText("等待共享");
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);

    initializeParticipantPanel();
    ensureParticipantTile(localParticipantId, localParticipantName, true);
    setParticipantPlaceholder(localParticipantId, QStringLiteral("我摄像头未打开"));

    setupMeetingControls();    updateResponsiveGeometry();
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
    clearRemoteReceivers();
    if (localSender) {
        localSender->setTransport(nullptr);
    }
    delete debugTransport;
    debugTransport = nullptr;
    delete ui;
}

