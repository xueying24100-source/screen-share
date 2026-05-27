#pragma once

#include <QMainWindow>
#include <QImage>
#include <QString>

#include "sharesourcepicker.h"

class AnnotationWindow;
class AudioCapturer;
class LocalPreviewWindow;
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
    void applyAnnotationGeometry();
    void ensurePreviewWindow();
    void updatePreviewPosition();
    void handleCaptureError(const QString& error);
    void onFrameCaptured(const QImage& frame);
    void refreshPreviewComposite();
    QImage composeFrameWithAnnotations(const QImage& frame) const;

    ScreenCapturer* m_capturer{nullptr};
    Sender* m_sender{nullptr};
    AudioCapturer* m_audioCapturer{nullptr};
    SystemAudioCapturer* m_systemAudioCapturer{nullptr};
    AnnotationWindow* m_annotationWindow{nullptr};
    LocalPreviewWindow* m_preview{nullptr};
    ShareToolbar* m_toolbar{nullptr};
    QTimer* m_windowFollowTimer{nullptr};
    QTimer* m_previewRefreshTimer{nullptr};

    QPushButton* m_shareButton{nullptr};
    QPushButton* m_endButton{nullptr};

    bool m_sharing{false};
    bool m_shareStartPending{false};
    int m_shareStartRequestId{0};
    QString m_lastCaptureError;
    ShareSelection m_currentSelection;
    QImage m_lastRawFrame;
};
