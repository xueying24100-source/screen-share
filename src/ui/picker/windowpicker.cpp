#include "windowpicker.h"
#include "ui_windowpicker.h"
#include <QDebug>
#include <QPixmap>
#include <QApplication>
#include <QThread>

struct EnumWindowsData
{
    QList<WindowInfo> *windowList;
    HWND skipHwnd;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsData *data = reinterpret_cast<EnumWindowsData *>(lParam);

    if (hwnd == data->skipHwnd) {
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return TRUE;
    }//过滤工具栏窗口

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    if (wcslen(title) == 0)
        return TRUE;

    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);

    const wchar_t* filteredClasses[] = {
        L"Progman",//桌面
        L"WorkerW",//桌面壁纸
        L"Shell_TrayWnd",//任务栏
        L"Shell_SecondaryTrayWnd",
        L"IME",//输入法
        L"MSCTFIME UI",
        L"tooltips_class32",//鼠标悬浮时的提示窗口
    };
    for (const auto& filtered : filteredClasses) {
        if (wcscmp(className, filtered) == 0) {
            return TRUE;
        }
    }

    WindowInfo info;
    info.hwnd = hwnd;
    info.title = QString::fromWCharArray(title);
    info.className = QString::fromWCharArray(className);
    data->windowList->append(info);
    return TRUE;
}

QList<WindowInfo> WindowPicker::enumerateWindows()
{
    QList<WindowInfo> windowList;
    EnumWindowsData data;
    data.windowList = &windowList;
    data.skipHwnd = reinterpret_cast<HWND>(winId());
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return windowList;
}

WindowPicker::WindowPicker(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::WindowPicker)
    , m_selectedWindow(nullptr)
    , m_capturer(nullptr)
    , m_isSharing(false)
{
    ui->setupUi(this);
    setWindowTitle("选择要共享的窗口");
    setMinimumSize(500, 600);
    m_capturer = new ScreenCapturer(this);
    connect(ui->refreshButton, &QPushButton::clicked,
            this, &WindowPicker::on_refreshButton_clicked);
    connect(ui->windowListWidget, &QListWidget::itemClicked,
            this, &WindowPicker::on_windowListWidget_itemClicked);
    connect(ui->startShareButton, &QPushButton::clicked,
            this, &WindowPicker::on_startShareButton_clicked);
    connect(ui->stopShareButton, &QPushButton::clicked,
            this, &WindowPicker::on_stopShareButton_clicked);
    connect(m_capturer, &ScreenCapturer::frameCaptured,
            this, &WindowPicker::onFrameCaptured);
    connect(m_capturer, &ScreenCapturer::captureError,
            [](const QString& msg) { qDebug() << "采集错误:" << msg; });

    ui->startShareButton->setEnabled(false);
    ui->stopShareButton->setEnabled(false);
    if (ui->previewLabel) {
        ui->previewLabel->setText("未采集");
        ui->previewLabel->setAlignment(Qt::AlignCenter);
        ui->previewLabel->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
        ui->previewLabel->setMinimumHeight(200);
    }

    on_refreshButton_clicked();
}

WindowPicker::~WindowPicker()
{
    if (m_capturer) {
        m_capturer->stop();
    }
    delete ui;
}

void WindowPicker::on_refreshButton_clicked()
{
    ui->windowListWidget->clear();

    ui->windowListWidget->setViewMode(QListView::IconMode);
    ui->windowListWidget->setFlow(QListView::LeftToRight);
    ui->windowListWidget->setWrapping(true);
    ui->windowListWidget->setSpacing(12);
    ui->windowListWidget->setWordWrap(true);

    int gridWidth = 230;
    int gridHeight = 140;
    int iconWidth = gridWidth - 10;
    int iconHeight = iconWidth * 95 / 190;

    ui->windowListWidget->setGridSize(QSize(gridWidth, gridHeight));
    ui->windowListWidget->setIconSize(QSize(iconWidth, iconHeight));
    ui->windowListWidget->setMinimumWidth(gridWidth * 2 + 36);

    QList<WindowInfo> windows = enumerateWindows();

    for (const auto &win : windows) {
        QImage thumbnail = ScreenCapturer::captureWindowOnce(
            reinterpret_cast<quintptr>(win.hwnd), QSize(iconWidth, iconHeight));

        QListWidgetItem *item = new QListWidgetItem();
        item->setText(win.title);
        item->setData(Qt::UserRole, QVariant::fromValue(win.hwnd));

        if (!thumbnail.isNull()) {
            item->setIcon(QPixmap::fromImage(thumbnail));
        }

        item->setTextAlignment(Qt::AlignCenter);
        ui->windowListWidget->addItem(item);
    }

    if (windows.isEmpty()) {
        ui->windowListWidget->addItem("未找到任何窗口");
    }
}

void WindowPicker::on_windowListWidget_itemClicked(QListWidgetItem *item)
{
    m_selectedWindow = item->data(Qt::UserRole).value<HWND>();
    m_selectedTitle = item->text();

    qDebug() << "选中窗口：" << m_selectedTitle << "，句柄：" << m_selectedWindow;

    if (IsIconic(m_selectedWindow)) {
        qDebug() << "窗口已最小化，无法采集";
        setWindowTitle(QString("窗口已最小化: %1").arg(m_selectedTitle));
        return;
    }

    if (m_isSharing) {
        m_capturer->stop();
    }

    QSize previewSize = ui->previewLabel->size();
    if (previewSize.width() <= 0 || previewSize.height() <= 0) {
        previewSize = QSize(640, 360);
    }
    m_capturer->setOutputSize(previewSize);
    qDebug() << "Set output size to:" << previewSize;

    m_capturer->startWindow(reinterpret_cast<quintptr>(m_selectedWindow), 15);
    m_isSharing = true;

    updateUIForSharing(true);
    setWindowTitle(QString("正在采集: %1").arg(m_selectedTitle));

    qDebug() << "开始共享窗口：" << m_selectedTitle;
}

void WindowPicker::on_startShareButton_clicked()
{
    if (!m_selectedWindow) {
        qDebug() << "请先选择一个窗口";
        return;
    }

    //获取预览区域的最终大小
    QSize previewSize = ui->previewLabel->size();
    if (previewSize.width() <= 0 || previewSize.height() <= 0) {
        // 如果大小无效，使用默认大小
        previewSize = QSize(640, 360);
        qDebug() << "Preview size invalid, using default:" << previewSize;
    }

    qDebug() << "Using preview size:" << previewSize;

    m_capturer->startWindow(reinterpret_cast<quintptr>(m_selectedWindow), 15);

    m_isSharing = true;
    updateUIForSharing(true);

    qDebug() << "开始共享窗口：" << m_selectedTitle;
}

void WindowPicker::on_stopShareButton_clicked()
{
    if (m_capturer) {
        m_capturer->stop();
    }

    m_isSharing = false;
    updateUIForSharing(false);

    if (ui->previewLabel) {
        ui->previewLabel->clear();
        ui->previewLabel->setText("未采集");
    }

    qDebug() << "停止共享";
}

void WindowPicker::onFrameCaptured(const QImage &frame)
{
    if (!frame.isNull() && ui->previewLabel) {
        ui->previewLabel->setPixmap(QPixmap::fromImage(frame));
    }
}

void WindowPicker::updateUIForSharing(bool isSharing)
{
    ui->startShareButton->setEnabled(!isSharing);
    ui->stopShareButton->setEnabled(isSharing);
    ui->refreshButton->setEnabled(!isSharing);
    ui->windowListWidget->setEnabled(!isSharing);
}