#include "mainwindow_impl_includes.h"

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
        Qt::SmoothTransformation);
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

void MainWindow::initializeParticipantPanel()
{
    if (!ui || !ui->topUserArea || participantScrollArea) {
        return;
    }

    // 参会者栏也已经写进 mainwindow.ui。运行时只负责往 participantLayout 里动态增删用户卡片。
    participantScrollArea = ui->participantScrollArea;
    participantContainer = ui->participantContainer;
    participantLayout = ui->participantLayout;

    if (ui->labelParticipantHint) {
        participantLayout->removeWidget(ui->labelParticipantHint);
        ui->labelParticipantHint->hide();
    }

    if (participantScrollArea) {
        participantScrollArea->setWidgetResizable(true);
        participantScrollArea->setFrameShape(QFrame::NoFrame);
        participantScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        participantScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        participantScrollArea->setStyleSheet(QStringLiteral(
            "QScrollArea#participantScrollArea { background: transparent; border: none; }"
            "QScrollBar:horizontal { height: 8px; background: #f1f5f9; border-radius: 4px; }"
            "QScrollBar::handle:horizontal { background: #cbd5e1; border-radius: 4px; min-width: 32px; }"));
    }
    if (participantContainer) {
        participantContainer->setStyleSheet(QStringLiteral("QWidget#participantContainer { background: transparent; }"));
    }
    if (participantLayout) {
        participantLayout->setContentsMargins(0, 0, 0, 0);
        participantLayout->setSpacing(14);
        participantLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
}

QLabel *MainWindow::ensureParticipantTile(const QString &userId, const QString &userName, bool local)
{
    if (userId.isEmpty()) {
        return nullptr;
    }
    initializeParticipantPanel();

    if (participantTiles.contains(userId)) {
        if (!userName.isEmpty()) {
            participantNames[userId] = userName;
        }
        return participantTiles.value(userId);
    }

    const QString displayName = userName.isEmpty() ? userId : userName;
    participantNames[userId] = displayName;

    QLabel *tile = new QLabel(participantContainer ? participantContainer : ui->topUserArea);
    tile->setObjectName(QStringLiteral("participantTile"));
    tile->setMinimumSize(160, 96);
    tile->setMaximumSize(220, 112);
    tile->setAlignment(Qt::AlignCenter);
    tile->setWordWrap(true);
    tile->setText(local ? QStringLiteral("%1\n摄像头未打开").arg(displayName)
                        : QStringLiteral("%1\n等待摄像头").arg(displayName));
    tile->setStyleSheet(QStringLiteral(
        "QLabel#participantTile {"
        " background-color: #ffffff;"
        " border: 1px solid #dcdfe6;"
        " border-radius: 12px;"
        " color: #334155;"
        " font-size: 15px;"
        " font-weight: bold;"
        " padding: 4px;"
        "}"));

    participantTiles.insert(userId, tile);
    if (participantLayout) {
        participantLayout->addWidget(tile);
    }
    return tile;
}

void MainWindow::removeParticipantTile(const QString &userId)
{
    QLabel *tile = participantTiles.take(userId);
    participantNames.remove(userId);
    if (tile) {
        if (participantLayout) {
            participantLayout->removeWidget(tile);
        }
        tile->deleteLater();
    }
    if (remoteReceivers.contains(userId)) {
        MediaReceiver *receiver = remoteReceivers.take(userId);
        receiver->deleteLater();
    }
}

void MainWindow::setParticipantPlaceholder(const QString &userId, const QString &text)
{
    const QString name = participantNames.value(userId, userId == localParticipantId ? localParticipantName : userId);
    QLabel *tile = ensureParticipantTile(userId, name, userId == localParticipantId);
    if (!tile) {
        return;
    }
    tile->clear();
    tile->setPixmap(QPixmap());
    tile->setText(text);
    tile->setAlignment(Qt::AlignCenter);
}

void MainWindow::updateParticipantVideo(const QString &userId, const QImage &image)
{
    const QString name = participantNames.value(userId, userId == localParticipantId ? localParticipantName : userId);
    QLabel *tile = ensureParticipantTile(userId, name, userId == localParticipantId);
    if (!tile) {
        return;
    }
    if (image.isNull()) {
        setParticipantPlaceholder(userId, QStringLiteral("%1\n摄像头未打开").arg(name));
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image).scaled(tile->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    tile->setText(QString());
    tile->setPixmap(pixmap);
    tile->setAlignment(Qt::AlignCenter);
}

MediaReceiver *MainWindow::receiverForPeer(const QString &userId)
{
    if (userId.isEmpty()) {
        return nullptr;
    }
    if (remoteReceivers.contains(userId)) {
        return remoteReceivers.value(userId);
    }

    MediaReceiver *receiver = new MediaReceiver(this);
    remoteReceivers.insert(userId, receiver);

    connect(receiver, &MediaReceiver::mainVideoFrameReceived, this, [this, userId](const QImage &image) {
        if (image.isNull() || sharing) {
            return;
        }
        currentRemoteSharerId = userId;
        onRemoteMainFrame(image);
    });
    connect(receiver, &MediaReceiver::mainVideoStoppedReceived, this, [this, userId]() {
        onRemoteMainStopped(userId);
    });
    connect(receiver, &MediaReceiver::cameraFrameReceived, this, [this, userId](const QImage &image) {
        updateParticipantVideo(userId, image);
    });
    connect(receiver, &MediaReceiver::cameraStoppedReceived, this, [this, userId]() {
        const QString name = participantNames.value(userId, userId);
        setParticipantPlaceholder(userId, QStringLiteral("%1\n摄像头已关闭").arg(name));
    });
    connect(receiver, &MediaReceiver::audioFrameReceived, this, [this](const QByteArray &pcm) {
        if (!localAudioPlayer || pcm.isEmpty()) {
            return;
        }
        if (!localAudioPlayer->isRunning()) {
            localAudioPlayer->start();
        }
        localAudioPlayer->playData(pcm);
    });
    connect(receiver, &MediaReceiver::receiverMessage, this, [this, userId](const QString &message) {
        qWarning() << "[Receiver]" << userId << message;
    });
    return receiver;
}

void MainWindow::clearRemoteReceivers()
{
    const auto receivers = remoteReceivers;
    remoteReceivers.clear();
    for (MediaReceiver *receiver : receivers) {
        if (receiver) {
            receiver->deleteLater();
        }
    }
}

