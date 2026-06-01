#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFrame>
#include <QList>
#include <QString>
#include <QRect>
#include <QIcon>
#include <QImage>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QToolButton;
class QPushButton;
class QCheckBox;
class QGridLayout;
class QLabel;
class QPixmap;
class QMoveEvent;
class AnnotationWindow;
class AudioCapturer;
class SystemAudioCapturer;
class AudioMixer;
class Sender;
class DebugTransport;
class ShareToolbar;
class AudioPlayer;
class CameraManager;
class TcpPacketTransport;
class MediaReceiver;
class INetworkTransport;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct WindowItem {
        quintptr handle = 0;
        QString title;
        bool minimized = false;
    };

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
    void toggleCamera();          // 打开/关闭本机摄像头
    void startHostMeeting();      // 第一版：作为主机监听本机/局域网连接
    void connectLocalMeeting();   // 第一版：连接 127.0.0.1 主机
    void onLocalCameraFrame(const QImage &image);
    void onRemoteMainFrame(const QImage &image);
    void onRemoteCameraFrame(const QImage &image);

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
    QList<WindowItem> listOpenWindows() const;
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
    QPixmap composeCursorOnPixmap(const QPixmap &basePixmap) const;
    void startLocalMediaBackend();
    void stopLocalMediaBackend();
    void updateShareTimerInterval();
    void sendFrameToLocalSender(const QPixmap &pixmap);
    void connectAnnotationToSender(AnnotationWindow *window);
    void setupMeetingControls();
    void layoutMeetingControls();
    INetworkTransport *activeTransport() const;
    void ensureSenderRunningForCamera();
    void updateVideoLabel(QLabel *label, const QImage &image, const QString &fallbackText);
    void setSmallLabelPlaceholder(QLabel *label, const QString &text);
    QIcon makeScreenThumbnailIcon(int screenIndex) const;
    void showShareToolbar();
    void hideShareToolbar();
    void positionShareToolbar();
    void setAnnotationEditingEnabled(bool enabled);
    QString optionSummary() const;
    QString shortWindowTitle(const QString &title, int maxLen = 14) const;

private:
    Ui::MainWindow *ui;
    QTimer *shareTimer = nullptr;
    bool sharing = false;
    bool sharePaused = false;
    bool micMuted = false;
    bool cameraOn = false;
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
    QCheckBox *checkShareAudio = nullptr;
    QCheckBox *checkShowCursor = nullptr;
    QCheckBox *checkSmooth = nullptr;
    QList<QToolButton*> windowSourceButtons;

    ShareToolbar *shareToolbar = nullptr;
    AudioPlayer *localAudioPlayer = nullptr;

    QPushButton *btnCamera = nullptr;
    QPushButton *btnHost = nullptr;
    QPushButton *btnConnectLocal = nullptr;

    AnnotationWindow *annotationWindow = nullptr;

    AudioCapturer *micCapturer = nullptr;
    SystemAudioCapturer *systemAudioCapturer = nullptr;
    AudioMixer *audioMixer = nullptr;
    Sender *localSender = nullptr;
    DebugTransport *debugTransport = nullptr;
    CameraManager *cameraManager = nullptr;
    TcpPacketTransport *networkTransport = nullptr;
    MediaReceiver *mediaReceiver = nullptr;
};

#endif
