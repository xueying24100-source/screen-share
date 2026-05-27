#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QString>
#include <QThread>

#include "sharesourcepicker.h"

class AnnotationWindow;
class AudioCapturer;
class AudioMixer;
class AudioPlayer;
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
    QImage composeFrameWithAnnotations(const QImage& frame);
    void onMixedAudio(const QByteArray& pcm);

    ScreenCapturer* m_capturer{nullptr};
    Sender* m_sender{nullptr};
    AudioCapturer* m_audioCapturer{nullptr};
    SystemAudioCapturer* m_systemAudioCapturer{nullptr};
    AudioMixer* m_audioMixer{nullptr};
    AudioPlayer* m_localPlayback{nullptr};
    AnnotationWindow* m_annotationWindow{nullptr};
    LocalPreviewWindow* m_preview{nullptr};
    ShareToolbar* m_toolbar{nullptr};
    QTimer* m_windowFollowTimer{nullptr};
    QTimer* m_previewRefreshTimer{nullptr};

    QThread* m_audioThread{nullptr};
    QThread* m_captureThread{nullptr};

    QPushButton* m_shareButton{nullptr};
    QPushButton* m_endButton{nullptr};

    bool m_sharing{false};
    bool m_shareStartPending{false};
    int m_shareStartRequestId{0};
    QString m_lastCaptureError;
    ShareSelection m_currentSelection;
    QImage m_lastRawFrame;
    QImage m_annotationLayerCache;
    QSize m_annotationLayerCacheSourceSize;
    QString m_lastFrameBackend;
    QElapsedTimer m_frameLogTimer;
    QElapsedTimer m_mixedAudioLogTimer;
    bool m_localPlaybackWarningShown{false};
    bool m_annotationLayerDirty{true};
};
