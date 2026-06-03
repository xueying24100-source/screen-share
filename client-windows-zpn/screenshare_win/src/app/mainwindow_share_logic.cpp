#include "mainwindow_impl_includes.h"

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
    updateResponsiveGeometry();
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

    shareTimer->start();
    captureScreen();
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

    // 窗口共享优先保证“实时看到的内容”。Chrome / Qt Creator / 飞书这类硬件加速窗口
    // 用 PrintWindow 容易停在初始帧，所以选择窗口后把目标窗口恢复并短暂置前，
    // 后续抓取会优先尝试屏幕实时裁剪。
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
#endif

    if (sharePopup) {
        sharePopup->hide();
    }

    if (shareTimer) {
        shareTimer->stop();
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

    shareTimer->start();
    captureScreen();
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

void MainWindow::captureScreen()
{
    if (!sharing || sharePaused) {
        return;
    }

    // 目标窗口或主窗口位置变化时，同步画笔层的位置，保证笔迹坐标和共享图像坐标一致。
    updateAnnotationWindowGeometry();

    if (currentShareType == ShareSourceType::Whiteboard) {
        updateWhiteboardPreview();
        return;
    }

    QPixmap pixmap;

    if (currentShareType == ShareSourceType::Screen) {
        const QList<QScreen*> screens = ScreenCapturer::screens();
        if (currentScreenIndex < 0 || currentScreenIndex >= screens.size()) {
            ui->labelStatus->setText("状态：扩展屏已断开");
            return;
        }

        QImage screenImage = ScreenCapturer::captureScreenOnce(currentScreenIndex);
        if (!screenImage.isNull()) {
            pixmap = QPixmap::fromImage(screenImage);
        }
    } else if (currentShareType == ShareSourceType::Window) {
        if (currentWindowHandle == 0) {
            return;
        }

#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(currentWindowHandle);
        if (!IsWindow(hwnd)) {
            ui->labelStatus->setText("状态：共享窗口已关闭");
            return;
        }
#endif

        QImage windowImage = ScreenCapturer::captureWindowOnce(currentWindowHandle);
        if (!windowImage.isNull()) {
            pixmap = QPixmap::fromImage(windowImage);
        }
    }

    if (pixmap.isNull()) {
        ui->labelMainScreen->clear();
        ui->labelMainScreen->setText("暂时无法获取共享画面");
        return;
    }

    pixmap = composeCursorOnPixmap(pixmap);
    pixmap = composeAnnotationOnPixmap(pixmap);

    ui->labelMainScreen->setStyleSheet("");
    updatePreviewWithPixmap(pixmap);
}

void MainWindow::endShare()
{
    // 先向远端发送“结束共享”控制包，再停止 sender/采集链路。
    // 否则远端会停留在最后一帧共享图像。
    if (sharing && localSender) {
        localSender->setTransport(activeTransport());
        localSender->sendShareStopped();
    }

    stopLocalMediaBackend();

    if (shareTimer) {
        shareTimer->stop();
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
        setParticipantPlaceholder(localParticipantId, QStringLiteral("我\n摄像头未打开"));
    }
    // 多人会议中停止共享不清空远端摄像头小窗。
}

