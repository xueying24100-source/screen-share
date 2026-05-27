#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFrame>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QToolButton;
class QCheckBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onShowSharePopup();     // 点击“开始共享”
    void onSelectDesktop1();     // 选择桌面1
    void onSelectDesktop2();     // 选择桌面2
    void onSelectWhiteboard();   // 选择白板（先做占位）
    void onSelectCurrentWindow();// 选择当前窗口（先做占位）
    void captureScreen();        // 定时抓屏
    void endShare();             // 结束共享

private:
    void createSharePopup();
    void centerSharePopup();
    void startShareByName(const QString &sourceName);

private:
    Ui::MainWindow *ui;
    QTimer *shareTimer = nullptr;
    bool sharing = false;

    QFrame *sharePopup = nullptr;
};

#endif