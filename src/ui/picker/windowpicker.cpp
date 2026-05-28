#include "windowpicker.h"
#include <QDebug>
#include "ui_windowpicker.h"

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    QList<WindowInfo> *windowList = reinterpret_cast<QList<WindowInfo> *>(lParam);

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    if (wcslen(title) == 0)
        return TRUE;

    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    if (wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0
        || wcscmp(className, L"Shell_TrayWnd") == 0) {
        return TRUE;
    }

    WindowInfo info;
    info.hwnd = hwnd;
    info.title = QString::fromWCharArray(title);
    info.className = QString::fromWCharArray(className);
    windowList->append(info);
    return TRUE;
}

QList<WindowInfo> WindowPicker::enumerateWindows()
{
    QList<WindowInfo> windowList;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windowList));
    return windowList;
}

WindowPicker::WindowPicker(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::WindowPicker)
    , m_selectedWindow(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("选择要共享的窗口");
}

WindowPicker::~WindowPicker()
{
    delete ui;
}

void WindowPicker::on_refreshButton_clicked()
{
    ui->windowListWidget->clear();
    QList<WindowInfo> windows = enumerateWindows();

    for (const auto &win : windows) {
        QListWidgetItem *item = new QListWidgetItem(win.title);
        item->setData(Qt::UserRole, QVariant::fromValue(win.hwnd));
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
    accept();
}