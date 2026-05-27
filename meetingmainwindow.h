#pragma once

#include <QMainWindow>

#include "sharesourcepicker.h"

class AnnotationOverlay;
class AudioCapturer;
class QPushButton;
class QTimer;
class ScreenCapturer;
class Sender;
class ShareToolbar;
class SystemAudioCapturer;

class MeetingMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MeetingMainWindow(QWidget* parent = nullptr);
    ~MeetingMainWindow() override;

private:
    void startSharing(const ShareSelection& selection);
    void stopSharing();
    void updateToolbarPosition();
    void updateOverlayGeometry();

    ScreenCapturer* m_capturer{nullptr};
    Sender* m_sender{nullptr};
    AudioCapturer* m_audioCapturer{nullptr};
    SystemAudioCapturer* m_systemAudioCapturer{nullptr};
    AnnotationOverlay* m_overlay{nullptr};
    ShareToolbar* m_toolbar{nullptr};
    QTimer* m_windowFollowTimer{nullptr};

    QPushButton* m_shareButton{nullptr};
    QPushButton* m_endButton{nullptr};

    bool m_sharing{false};
    ShareSelection m_currentSelection;
};
