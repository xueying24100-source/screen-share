#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFrame>
#include <QList>
#include <QString>
#include <QRect>

#include "sourceenumerator.h"
#include "screencapturer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QToolButton;
class QCheckBox;
class QGridLayout;
class QLabel;
class QPixmap;
class QMoveEvent;
class AnnotationWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void onShowSharePopup();      // 点击“开始共享”
    void onSelectDesktop1();      // 选择桌面1
    void onSelectDesktop2();      // 选择桌面2 / 扩展屏
    void onSelectWhiteboard();    // 选择白板共享
    void onSelectCurrentWindow(); // 兼容旧入口：选择当前活动窗口
    void captureScreen();         // 定时抓屏
    void endShare();              // 结束共享
    void toggleAnnotationWindow();// 共享过程中打开/关闭画笔标注

private:
    enum class ShareSourceType {
        None,
        Screen,
        Whiteboard,
        Window
    };

    void createSharePopup();
    void refreshSharePopupOptions();
    void clearWindowButtons();
    void centerSharePopup();

    void startShareScreen(int screenIndex);
    void startShareWhiteboard();
    void startShareWindow(quintptr windowHandle, const QString &windowTitle);
    void startShareByName(const QString &sourceName); // 兼容旧逻辑

    void showAnnotationWindow();
    void updatePreviewWithPixmap(const QPixmap &pixmap);
    void showWhiteboardPreview();
    void updateWhiteboardPreview();
    QPixmap buildWhiteboardPixmap() const;
    QPixmap captureWindowPixmap(quintptr windowHandle) const;
    bool pixmapLooksMostlyBlack(const QPixmap &pixmap) const;
    QRect whiteboardGlobalRect() const;
    QRect windowGlobalRect(quintptr windowHandle) const;
    QRect annotationTargetGlobalRect() const;
    void updateAnnotationWindowGeometry();
    QPixmap composeAnnotationOnPixmap(const QPixmap &basePixmap) const;
    QString shortWindowTitle(const QString &title, int maxLen = 14) const;

private:
    Ui::MainWindow *ui;
    QTimer *shareTimer = nullptr;
    bool sharing = false;
    QString currentShareSource;
    ShareSourceType currentShareType = ShareSourceType::None;
    int currentScreenIndex = 0;
    quintptr currentWindowHandle = 0;

    QFrame *sharePopup = nullptr;
    QGridLayout *shareGrid = nullptr;
    QGridLayout *windowGrid = nullptr;
    QLabel *windowGroupLabel = nullptr;
    QToolButton *btnDesktop1 = nullptr;
    QToolButton *btnDesktop2 = nullptr;
    QToolButton *btnWhiteboard = nullptr;
    QToolButton *btnCurrentWindow = nullptr;
    QList<QToolButton*> windowSourceButtons;

    AnnotationWindow *annotationWindow = nullptr;
    ScreenCapturer *screenCapturer = nullptr;
};

#endif
