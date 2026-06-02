#include "mainwindow_impl_includes.h"

void MainWindow::setupMeetingControls()
{
    // v8.1 起，创建会议 / 加入会议 / 打开摄像头按钮已经写进 mainwindow.ui。
    // 这里不再动态 new 按钮，只拿 UI 文件里的控件指针，避免 Qt Designer 和运行界面不一致。
    if (!ui || !ui->bottomBar) {
        return;
    }

    btnHost = ui->btnHostMeeting;
    btnConnectLocal = ui->btnConnectLocal;
    btnCamera = ui->btnCamera;

    if (btnHost) {
        btnHost->show();
    }
    if (btnConnectLocal) {
        btnConnectLocal->show();
    }
    if (btnCamera) {
        btnCamera->show();
    }

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
    // v8.3：状态区域单独占一整行，避免会议码和端口信息被按钮挤掉。
    const int statusY = 8;
    const int statusH = 38;
    const int y = 58;
    const int h = 40;
    const int rightMargin = 22;
    int gap = 22;

    const QList<int> widths = {105, 105, 100, 90, 100, 115};
    int totalButtonsW = 0;
    for (int w : widths) {
        totalButtonsW += w;
    }
    int totalW = totalButtonsW + gap * (widths.size() - 1);

    // 按钮放到第二行居中；窗口较窄时压缩间距，避免重叠。
    if (totalW + rightMargin * 2 > barW) {
        gap = qMax(10, (barW - rightMargin * 2 - totalButtonsW) / (widths.size() - 1));
        totalW = totalButtonsW + gap * (widths.size() - 1);
    }

    int x = qMax(rightMargin, (barW - totalW) / 2);
    ui->labelStatus->setGeometry(20, statusY, qMax(300, barW - 40), statusH);
    ui->labelStatus->setWordWrap(true);
    ui->labelStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

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

void MainWindow::startHostMeeting()
{
    if (!networkTransport) {
        return;
    }

    constexpr quint16 kDefaultPort = 9000;
    const QString meetingCode = QString::number(100000 + QRandomGenerator::global()->bounded(900000));

    if (networkTransport->listen(kDefaultPort, meetingCode)) {
        if (localSender) {
            localSender->setTransport(activeTransport());
        }

        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("会议已创建"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(QStringLiteral("<div style='font-size:16px;'>会议创建成功</div>"
                                      "<div style='margin-top:10px; font-size:28px; font-weight:700; color:#1677ff;'>会议码：%1</div>"
                                      "<div style='margin-top:14px; font-size:14px; color:#475569;'>请让其他窗口点击“加入会议”，然后输入这个会议码。<br/>"
                                      "当前本机多开测试默认连接 127.0.0.1:9000。</div>").arg(meetingCode));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWindow::connectLocalMeeting()
{
    if (!networkTransport) {
        return;
    }

    bool ok = false;
    QString meetingCode = QInputDialog::getText(this,
                                                QStringLiteral("加入会议"),
                                                QStringLiteral("请输入会议码："),
                                                QLineEdit::Normal,
                                                QString(),
                                                &ok);
    if (!ok) {
        return;
    }
    meetingCode = meetingCode.trimmed();
    if (meetingCode.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加入会议"), QStringLiteral("会议码不能为空。"));
        return;
    }

    constexpr quint16 kDefaultPort = 9000;
    networkTransport->connectToPeer(QStringLiteral("127.0.0.1"), kDefaultPort, meetingCode);
}

void MainWindow::onMeetingPacketReceived(const QString &senderId, const QByteArray &packet)
{
    if (senderId.isEmpty() || senderId == localParticipantId) {
        return;
    }
    MediaReceiver *receiver = receiverForPeer(senderId);
    if (receiver) {
        receiver->onPacketReceived(packet);
    }
}

void MainWindow::onParticipantJoined(const QString &userId, const QString &userName)
{
    if (userId.isEmpty()) {
        return;
    }
    const bool isLocal = (userId == localParticipantId);
    QLabel *tile = ensureParticipantTile(userId, userName, isLocal);
    if (tile && !cameraOn && isLocal) {
        setParticipantPlaceholder(userId, QStringLiteral("%1\n摄像头未打开").arg(userName.isEmpty() ? QStringLiteral("我") : userName));
    }
    ui->labelStatus->setText(QStringLiteral("状态：%1 加入会议，当前人数：%2")
                             .arg(userName.isEmpty() ? userId : userName)
                             .arg(participantTiles.size()));
}

void MainWindow::onParticipantLeft(const QString &userId)
{
    if (userId.isEmpty() || userId == localParticipantId) {
        return;
    }
    const QString name = participantNames.value(userId, userId);
    removeParticipantTile(userId);
    if (currentRemoteSharerId == userId) {
        currentRemoteSharerId.clear();
        if (!sharing) {
            ui->labelMainScreen->clear();
            ui->labelMainScreen->setText(QStringLiteral("等待共享"));
            ui->labelMainScreen->setAlignment(Qt::AlignCenter);
        }
    }
    ui->labelStatus->setText(QStringLiteral("状态：%1 已离开，当前人数：%2")
                             .arg(name)
                             .arg(participantTiles.size()));
}

void MainWindow::onLocalIdentityAssigned(const QString &userId, const QString &userName)
{
    if (userId.isEmpty()) {
        return;
    }

    const QString oldLocalId = localParticipantId;
    localParticipantId = userId;
    localParticipantName = userName.isEmpty() ? QStringLiteral("我") : userName;

    if (oldLocalId != localParticipantId && participantTiles.contains(oldLocalId)) {
        removeParticipantTile(oldLocalId);
    }

    ensureParticipantTile(localParticipantId, localParticipantName, true);
    if (!cameraOn) {
        setParticipantPlaceholder(localParticipantId,
                                  QStringLiteral("%1\n摄像头未打开").arg(localParticipantName));
    }
}

