#ifndef WINDOWPICKER_H
#define WINDOWPICKER_H

#include <QDialog>
#include <QList>
#include <QListWidgetItem>
#include <QLabel>
#include <windows.h>
#include "screencapturer.h"

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
    HWND getSelectedWindow() const { return m_selectedWindow; }
    QString getSelectedTitle() const { return m_selectedTitle; }

private slots:
    void on_refreshButton_clicked();
    void on_windowListWidget_itemClicked(QListWidgetItem *item);
    void on_startShareButton_clicked();
    void on_stopShareButton_clicked();
    void onFrameCaptured(const QImage &frame);

private:
    void updateUIForSharing(bool isSharing);

private:
    Ui::WindowPicker *ui;
    HWND m_selectedWindow;
    QString m_selectedTitle;
    ScreenCapturer *m_capturer;
    bool m_isSharing;
};

#endif