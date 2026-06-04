#include "mainwindow_impl_includes.h"

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
            updateWhiteboardPreview();
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
        Qt::SmoothTransformation
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
        setParticipantPlaceholder(localParticipantId, QStringLiteral("我\n摄像头未打开"));
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

