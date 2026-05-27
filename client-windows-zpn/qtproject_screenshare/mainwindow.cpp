#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    shareTimer = new QTimer(this);
    shareTimer->setInterval(100);   // 10fps，演示够用了

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

    connect(shareTimer, &QTimer::timeout,
            this, &MainWindow::captureScreen);
}

MainWindow::~MainWindow()
{
    if (shareTimer) {
        shareTimer->stop();
    }
    delete ui;
}

void MainWindow::createSharePopup()
{
    sharePopup = new QFrame(ui->centralwidget);
    sharePopup->setObjectName("sharePopup");
    sharePopup->setFixedSize(560, 500);
    sharePopup->hide();

    QVBoxLayout *mainLayout = new QVBoxLayout(sharePopup);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    QLabel *titleLabel = new QLabel("桌面与窗口", sharePopup);
    titleLabel->setObjectName("sharePopupTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);

    auto makeButton = [&](const QString &text, const QIcon &icon) {
        QToolButton *btn = new QToolButton(sharePopup);
        btn->setText(text);
        btn->setIcon(icon);
        btn->setIconSize(QSize(48, 48));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(150, 110);
        return btn;
    };

    QToolButton *btnDesktop1 =
        makeButton("桌面1", style()->standardIcon(QStyle::SP_DesktopIcon));

    QToolButton *btnDesktop2 =
        makeButton("桌面2", style()->standardIcon(QStyle::SP_ComputerIcon));

    QToolButton *btnWhiteboard =
        makeButton("白板", style()->standardIcon(QStyle::SP_FileDialogDetailedView));

    QToolButton *btnCurrentWindow =
        makeButton("当前窗口", style()->standardIcon(QStyle::SP_FileDialogContentsView));

    grid->addWidget(btnDesktop1, 0, 0);
    grid->addWidget(btnDesktop2, 0, 1);
    grid->addWidget(btnWhiteboard, 1, 0);
    grid->addWidget(btnCurrentWindow, 1, 1);

    mainLayout->addLayout(grid);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setSpacing(24);

    QCheckBox *checkShareAudio = new QCheckBox("共享音频", sharePopup);
    QCheckBox *checkSmooth = new QCheckBox("流畅模式", sharePopup);

    bottomLayout->addWidget(checkShareAudio);
    bottomLayout->addWidget(checkSmooth);
    bottomLayout->addStretch();

    mainLayout->addLayout(bottomLayout);

    connect(btnDesktop1, &QToolButton::clicked,
            this, &MainWindow::onSelectDesktop1);

    connect(btnDesktop2, &QToolButton::clicked,
            this, &MainWindow::onSelectDesktop2);

    connect(btnWhiteboard, &QToolButton::clicked,
            this, &MainWindow::onSelectWhiteboard);

    connect(btnCurrentWindow, &QToolButton::clicked,
            this, &MainWindow::onSelectCurrentWindow);

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
            font-size: 28px;
            font-weight: bold;
            color: #1f2329;
        }

        QToolButton {
            background-color: #f7f8fa;
            border: 1px solid #dcdfe6;
            border-radius: 12px;
            font-size: 16px;
            font-weight: bold;
            color: #1f2329;
            padding: 8px;
        }

        QToolButton:hover {
            background-color: #e8f1ff;
            border: 1px solid #1677ff;
            color: #1677ff;
        }

        QCheckBox {
            font-size: 15px;
            color: #333333;
        }
    )");
}

void MainWindow::centerSharePopup()
{
    if (!sharePopup) return;

    int x = (ui->centralwidget->width() - sharePopup->width()) / 2;
    int y = (ui->centralwidget->height() - sharePopup->height()) / 2 - 20;

    if (y < 20) y = 20;

    sharePopup->move(x, y);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    centerSharePopup();
}

void MainWindow::onShowSharePopup()
{
    if (sharing) {
        return;
    }

    centerSharePopup();
    sharePopup->show();
    sharePopup->raise();
}

void MainWindow::startShareByName(const QString &sourceName)
{
    sharePopup->hide();

    sharing = true;
    ui->labelStatus->setText("状态：正在共享 " + sourceName);
    ui->btnShare->setText("共享中");

    shareTimer->start();
}

void MainWindow::onSelectDesktop1()
{
    startShareByName("桌面1");
}

void MainWindow::onSelectDesktop2()
{
    startShareByName("桌面2");
}

void MainWindow::onSelectWhiteboard()
{
    QMessageBox::information(this, "提示", "白板功能先做展示占位，后续再接功能。");
}

void MainWindow::onSelectCurrentWindow()
{
    startShareByName("当前窗口");
}

void MainWindow::captureScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) return;

    QPixmap mainPixmap = pixmap.scaled(
        ui->labelMainScreen->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );

    ui->labelMainScreen->setPixmap(mainPixmap);

    QPixmap smallPixmap = pixmap.scaled(
        ui->labelSmall1->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );

    ui->labelSmall1->setPixmap(smallPixmap);
    ui->labelSmall2->setPixmap(smallPixmap);
    ui->labelSmall3->setPixmap(smallPixmap);
    ui->labelSmall4->setPixmap(smallPixmap);
    ui->labelSmall5->setPixmap(smallPixmap);
}

void MainWindow::endShare()
{
    if (shareTimer) {
        shareTimer->stop();
    }

    if (sharePopup) {
        sharePopup->hide();
    }

    sharing = false;
    ui->labelStatus->setText("状态：未共享");
    ui->btnShare->setText("开始共享");

    ui->labelMainScreen->clear();
    ui->labelMainScreen->setText("等待共享");

    ui->labelSmall1->clear(); ui->labelSmall1->setText("用户1");
    ui->labelSmall2->clear(); ui->labelSmall2->setText("用户2");
    ui->labelSmall3->clear(); ui->labelSmall3->setText("用户3");
    ui->labelSmall4->clear(); ui->labelSmall4->setText("用户4");
    ui->labelSmall5->clear(); ui->labelSmall5->setText("用户5");
}