#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "annotationwindow.h"

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
#include <QApplication>
#include <QWindow>

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
    shareTimer->setInterval(160);   // 约 6fps：降低窗口抓取与合成压力，demo 更流畅

    ui->labelStatus->setText("状态：未共享");
    ui->labelMainScreen->setText("等待共享");
    ui->labelMainScreen->setAlignment(Qt::AlignCenter);

    ui->labelSmall1->setAlignment(Qt::AlignCenter);
    ui->labelSmall2->setAlignment(Qt::AlignCenter);
    ui->labelSmall3->setAlignment(Qt::AlignCenter);
    ui->labelSmall4->setAlignment(Qt::AlignCenter);
    ui->labelSmall5->setAlignment(Qt::AlignCenter);

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

    screenCapturer = new ScreenCapturer(this);
    connect(screenCapturer, &ScreenCapturer::frameCaptured,
            this, [this](const QImage &frame) {
        if (!sharing || currentShareType == ShareSourceType::Whiteboard) {
            return;
        }
        QPixmap pixmap = QPixmap::fromImage(frame);
        pixmap = composeAnnotationOnPixmap(pixmap);
        ui->labelMainScreen->setStyleSheet("");
        updatePreviewWithPixmap(pixmap);
    });
    connect(screenCapturer, &ScreenCapturer::captureError,
            this, [this](const QString &msg) {
        ui->labelStatus->setText("状态：捕获错误 - " + msg);
    });
}

MainWindow::~MainWindow()
{
    if (shareTimer) {
        shareTimer->stop();
    }
    if (annotationWindow) {
        annotationWindow->close();
    }
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
        btn->setIconSize(QSize(48, 48));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(150, 110);
        btn->setAutoRaise(false);
        return btn;
    };

    btnDesktop1 = makeButton("桌面1", style()->standardIcon(QStyle::SP_DesktopIcon));
    btnDesktop2 = makeButton("桌面2", style()->standardIcon(QStyle::SP_ComputerIcon));
    btnWhiteboard = makeButton("白板", style()->standardIcon(QStyle::SP_FileDialogDetailedView));

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

    QCheckBox *checkShareAudio = new QCheckBox("共享音频", sharePopup);
    QCheckBox *checkSmooth = new QCheckBox("流畅模式", sharePopup);

    bottomLayout->addWidget(checkShareAudio);
    bottomLayout->addWidget(checkSmooth);
    bottomLayout->addStretch();

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
    if (btnDesktop2) {
        // 没有扩展屏时，直接隐藏“桌面2”。
        btnDesktop2->setVisible(screens.size() >= 2);
        btnDesktop2->setToolTip(screens.size() >= 2
                                    ? QStringLiteral("共享第二块屏幕 / 扩展屏")
                                    : QStringLiteral("未检测到扩展屏"));
    }

    clearWindowButtons();

    QList<WindowInfo> windows = SourceEnumerator::enumerateWindows();
    const int maxWindowButtons = 6;
    int count = 0;
    for (const WindowInfo &item : windows) {
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
        btn->setIcon(style()->standardIcon(item.minimized
                                               ? QStyle::SP_TitleBarMinButton
                                               : QStyle::SP_TitleBarNormalButton));
        btn->setIconSize(QSize(42, 42));
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

QString MainWindow::shortWindowTitle(const QString &title, int maxLen) const
{
    QString t = title.simplified();
    if (t.size() <= maxLen) {
        return t;
    }
    return t.left(maxLen) + QStringLiteral("...");
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    centerSharePopup();
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
    ui->labelStatus->setText("状态：正在共享 " + currentShareSource);
    ui->btnShare->setText("共享中");
    ui->btnAnnotate->setEnabled(true);
    ui->btnAnnotate->setText("画笔");

    screenCapturer->startScreen(screenIndex, 10);
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

    ui->labelStatus->setText("状态：正在共享白板");
    ui->btnShare->setText("共享中");
    ui->btnAnnotate->setEnabled(true);
    ui->btnAnnotate->setText("画笔");

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

    // 如果窗口在任务栏里最小化，先恢复窗口。
    // 注意：不要对所有窗口都强制 SetForegroundWindow，否则点击共享后会把目标窗口突然弹到最前面。
    // 只有最小化窗口需要恢复并临时置前，避免抓到黑屏或空内容。
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
    ui->labelStatus->setText("状态：正在共享 " + currentShareSource);
    ui->btnShare->setText("共享中");
    ui->btnAnnotate->setEnabled(true);
    ui->btnAnnotate->setText("画笔");

    screenCapturer->startWindow(windowHandle, 10);
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

    QPixmap mainPixmap = pixmap.scaled(
        ui->labelMainScreen->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation
        );

    ui->labelMainScreen->setPixmap(mainPixmap);

    QPixmap smallPixmap = pixmap.scaled(
        ui->labelSmall1->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation
        );

    ui->labelSmall1->setPixmap(smallPixmap);
    ui->labelSmall2->setPixmap(smallPixmap);
    ui->labelSmall3->setPixmap(smallPixmap);
    ui->labelSmall4->setPixmap(smallPixmap);
    ui->labelSmall5->setPixmap(smallPixmap);
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

    ui->labelSmall1->clear(); ui->labelSmall1->setText("白板");
    ui->labelSmall2->clear(); ui->labelSmall2->setText("用户2");
    ui->labelSmall3->clear(); ui->labelSmall3->setText("用户3");
    ui->labelSmall4->clear(); ui->labelSmall4->setText("用户4");
    ui->labelSmall5->clear(); ui->labelSmall5->setText("用户5");
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
        ui->labelMainScreen->clear();
        ui->labelMainScreen->setText(QString());
        ui->labelMainScreen->setAlignment(Qt::AlignCenter);

        QPixmap smallPixmap = whiteboard.scaled(
            ui->labelSmall1->size(),
            Qt::KeepAspectRatio,
            Qt::FastTransformation
            );

        ui->labelSmall1->setPixmap(smallPixmap);
        ui->labelSmall2->setPixmap(smallPixmap);
        ui->labelSmall3->setPixmap(smallPixmap);
        ui->labelSmall4->setPixmap(smallPixmap);
        ui->labelSmall5->setPixmap(smallPixmap);
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
    if (!sharing) {
        return;
    }

    // 目标窗口或主窗口位置变化时，同步画笔层的位置，保证笔迹坐标和共享图像坐标一致。
    updateAnnotationWindowGeometry();

    if (currentShareType == ShareSourceType::Whiteboard) {
        updateWhiteboardPreview();
        return;
    }

    // Screen 和 Window 模式由 ScreenCapturer 持续推帧，captureScreen() 不再抓图。
}

void MainWindow::endShare()
{
    if (shareTimer) {
        shareTimer->stop();
    }
    if (screenCapturer) {
        screenCapturer->stop();
    }

    if (sharePopup) {
        sharePopup->hide();
    }

    if (annotationWindow) {
        annotationWindow->hide();
        annotationWindow->deleteLater();
        annotationWindow = nullptr;
    }

    sharing = false;
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

    ui->labelSmall1->clear(); ui->labelSmall1->setText("用户1");
    ui->labelSmall2->clear(); ui->labelSmall2->setText("用户2");
    ui->labelSmall3->clear(); ui->labelSmall3->setText("用户3");
    ui->labelSmall4->clear(); ui->labelSmall4->setText("用户4");
    ui->labelSmall5->clear(); ui->labelSmall5->setText("用户5");
}
