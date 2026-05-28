#ifndef WINDOWPICKER_H
#define WINDOWPICKER_H

#include <QDialog>
#include <QList>
#include <QListWidgetItem>
#include <windows.h>

struct WindowInfo
{
    HWND hwnd;
    QString title;
    QString className;
};

QT_BEGIN_NAMESPACE
namespace Ui {
class WindowPicker;
}
QT_END_NAMESPACE

class WindowPicker : public QDialog
{
    Q_OBJECT

public:
    explicit WindowPicker(QWidget *parent = nullptr);
    ~WindowPicker();
    QList<WindowInfo> enumerateWindows();

private slots:
    void on_refreshButton_clicked();
    void on_windowListWidget_itemClicked(QListWidgetItem *item);

private:
    Ui::WindowPicker *ui;
    HWND m_selectedWindow;
    QString m_selectedTitle;
};

#endif