#include "mainwindow_impl_includes.h"

namespace {
#ifdef Q_OS_WIN
struct EnumWindowContext
{
    QList<MainWindow::WindowItem> *items = nullptr;
};

BOOL CALLBACK enumCapturableWindows(HWND hwnd, LPARAM lParam)
{
    auto *ctx = reinterpret_cast<EnumWindowContext*>(lParam);
    if (!ctx || !ctx->items) {
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    // 过滤掉工具窗口、无标题窗口、子窗口，尽量接近任务栏里能看到的窗口。
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return TRUE;
    }

    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return TRUE;
    }

    const int titleLen = GetWindowTextLengthW(hwnd);
    if (titleLen <= 0) {
        return TRUE;
    }

    wchar_t titleBuffer[512] = {0};
    GetWindowTextW(hwnd, titleBuffer, 511);
    QString title = QString::fromWCharArray(titleBuffer).trimmed();
    if (title.isEmpty()) {
        return TRUE;
    }

    // 排除一些系统外壳窗口，避免列表太乱。
    if (title == QStringLiteral("Program Manager") ||
        title == QStringLiteral("Windows 输入体验") ||
        title == QStringLiteral("Windows Input Experience")) {
        return TRUE;
    }

    RECT rect{};
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const bool minimized = IsIconic(hwnd);
    if (!minimized && (width < 120 || height < 80)) {
        return TRUE;
    }

    MainWindow::WindowItem item;
    item.handle = reinterpret_cast<quintptr>(hwnd);
    item.title = title;
    item.minimized = minimized;
    ctx->items->append(item);

    return TRUE;
}
#endif
}

void MainWindow::createSharePopup()
{
    sharePopup = new QFrame(ui->centralwidget);
    sharePopup->setObjectName("sharePopup");
    sharePopup->setFixedSize(820, 700);
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
        btn->setIconSize(QSize(132, 74));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(170, 120);
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
            localSender->setMaxFps(localSender->mainVideoStreamId(), smooth ? 12 : 8);
            localSender->setVideoQuality(localSender->mainVideoStreamId(), smooth ? 82 : 100);
            localSender->setVideoMaxSize(localSender->mainVideoStreamId(), smooth ? QSize(1280, 720) : QSize());
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
        btnDesktop1->setIconSize(QSize(132, 74));
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
        btnDesktop2->setIconSize(QSize(132, 74));
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
        btn->setIcon(makeWindowThumbnailIcon(item.handle, item.minimized));
        btn->setIconSize(QSize(124, 70));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(170, 120);

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

#ifdef Q_OS_WIN
    EnumWindowContext ctx;
    ctx.items = &items;
    EnumWindows(enumCapturableWindows, reinterpret_cast<LPARAM>(&ctx));
#else
    // Qt 本身没有跨平台枚举所有外部应用窗口的接口。
    // Windows 版本用 Win32 EnumWindows 实现，其他平台这里先返回空列表。
#endif

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

void MainWindow::updateResponsiveGeometry()
{
    if (!ui || !ui->widget || !ui->topUserArea || !ui->labelMainScreen || !ui->bottomBar) {
        return;
    }

    // mainwindow.ui 里保留了旧版绝对定位。最大化后若不手动重排，右侧会出现大片空白。
    // 这里把上方用户区、主共享区、底部控制栏全部按当前窗口尺寸重新布局。
    const int w = qMax(ui->widget->width(), 720);
    const int h = qMax(ui->widget->height(), 480);
    const int marginX = 18;
    const int topMargin = 10;
    const int topH = 122;
    const int gap = 14;
    const int bottomH = 112;
    const int bottomY = qMax(topMargin + topH + gap + 260, h - bottomH - 8);
    const int mainY = topMargin + topH + gap;
    const int mainH = qMax(260, bottomY - mainY - gap);

    ui->topUserArea->setGeometry(marginX, topMargin, qMax(100, w - marginX * 2), topH);
    ui->labelMainScreen->setGeometry(marginX, mainY, qMax(100, w - marginX * 2), mainH);
    ui->bottomBar->setGeometry(0, bottomY, w, bottomH);

    layoutMeetingControls();
}

QIcon MainWindow::makeScreenThumbnailIcon(int screenIndex) const
{
    const QSize iconSize(132, 74);
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

QPixmap MainWindow::captureVisibleWindowPixmap(quintptr windowHandle) const
{
    if (windowHandle == 0) {
        return QPixmap();
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    if (!IsWindow(hwnd) || IsIconic(hwnd)) {
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

    QPoint center((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
    QScreen *targetScreen = QGuiApplication::screenAt(center);
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (!targetScreen) {
        return QPixmap();
    }

    // 抓“屏幕上实时可见的窗口区域”。对于 Chrome / Qt Creator / 飞书等硬件加速窗口，
    // PrintWindow 可能只返回初始化帧；屏幕裁剪虽然要求目标窗口可见，但实时性最好。
    return targetScreen->grabWindow(0, rect.left, rect.top, width, height);
#else
    Q_UNUSED(windowHandle);
    return QPixmap();
#endif
}

QIcon MainWindow::makeWindowThumbnailIcon(quintptr windowHandle, bool minimized) const
{
    const QSize iconSize(124, 70);
    QPixmap canvas(iconSize);
    canvas.fill(Qt::transparent);

    QPixmap windowPixmap;
    if (!minimized && windowHandle != 0) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(windowHandle);
        if (IsWindow(hwnd)) {
            RECT rect{};
            if (GetWindowRect(hwnd, &rect)) {
                const int width = rect.right - rect.left;
                const int height = rect.bottom - rect.top;
                if (width > 0 && height > 0) {
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
                        captured = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
                        if (captured) {
                            BITMAPINFO bmi{};
                            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                            bmi.bmiHeader.biWidth = width;
                            bmi.bmiHeader.biHeight = -height;
                            bmi.bmiHeader.biPlanes = 1;
                            bmi.bmiHeader.biBitCount = 32;
                            bmi.bmiHeader.biCompression = BI_RGB;
                            captured = GetDIBits(memDc, bitmap, 0, height, image.bits(), &bmi, DIB_RGB_COLORS) != 0;
                        }
                    }
                    if (oldBitmap) SelectObject(memDc, oldBitmap);
                    if (bitmap) DeleteObject(bitmap);
                    if (memDc) DeleteDC(memDc);
                    if (screenDc) ReleaseDC(nullptr, screenDc);
                    if (captured) {
                        windowPixmap = QPixmap::fromImage(image);
                    }
                }
            }
        }
#else
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            windowPixmap = screen->grabWindow(static_cast<WId>(windowHandle));
        }
#endif
    }

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(canvas.rect().adjusted(1, 1, -1, -1), 8, 8);
    painter.setClipPath(path);

    if (!windowPixmap.isNull() && !pixmapLooksMostlyBlack(windowPixmap)) {
        QPixmap scaled = windowPixmap.scaled(iconSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = qMax(0, (scaled.width() - iconSize.width()) / 2);
        const int y = qMax(0, (scaled.height() - iconSize.height()) / 2);
        painter.drawPixmap(0, 0, scaled.copy(x, y, iconSize.width(), iconSize.height()));
    } else {
        painter.fillRect(canvas.rect(), QColor("#f8fafc"));
        painter.setPen(QPen(QColor("#111827"), 3));
        painter.setBrush(Qt::NoBrush);
        const QRect r(40, 18, 38, 30);
        painter.drawRect(r.translated(5, -5));
        painter.drawRect(r);
        if (minimized) {
            painter.setPen(QPen(QColor("#64748b"), 3));
            painter.drawLine(45, 52, 82, 52);
        }
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

