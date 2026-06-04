#include "mainwindow_impl_includes.h"

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

void MainWindow::onLocalCameraFrame(const QImage &image)
{
    updateParticipantVideo(localParticipantId, image);
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
        Qt::SmoothTransformation);
    ui->labelMainScreen->setStyleSheet("");
    ui->labelMainScreen->setPixmap(pixmap);
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);
}

void MainWindow::onRemoteMainStopped(const QString &userId)
{
    if (sharing) {
        return;
    }
    if (!currentRemoteSharerId.isEmpty() && currentRemoteSharerId != userId) {
        return;
    }

    currentRemoteSharerId.clear();
    ui->labelMainScreen->clear();
    ui->labelMainScreen->setStyleSheet("");
    ui->labelMainScreen->setText(QStringLiteral("等待共享"));
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);

    const QString name = participantNames.value(userId, userId);
    ui->labelStatus->setText(QStringLiteral("状态：%1 已结束共享").arg(name));
}

void MainWindow::onRemoteCameraFrame(const QImage &image)
{
    updateParticipantVideo(QStringLiteral("remote"), image);
}

void MainWindow::updateShareTimerInterval()
{
    if (!shareTimer) {
        return;
    }
    // 流畅模式：约 12fps，并降低发送分辨率；普通模式：约 8fps，优先保证高清。
    const bool smooth = checkSmooth && checkSmooth->isChecked();
    shareTimer->setInterval(smooth ? 80 : 125);
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
        localSender->setMaxFps(localSender->mainVideoStreamId(), smooth ? 12 : 8);
        localSender->setVideoQuality(localSender->mainVideoStreamId(), smooth ? 82 : 100);
        localSender->setVideoMaxSize(localSender->mainVideoStreamId(), smooth ? QSize(1280, 720) : QSize());
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

