#pragma once

#include <QPoint>
#include <QWidget>

class QPushButton;
class QCheckBox;
class QMouseEvent;
class QEnterEvent;
class QEvent;

class ShareToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit ShareToolbar(QWidget* parent = nullptr);

    void setPaused(bool paused);
    void setAnnotationEnabled(bool enabled);
    void setMicMuted(bool muted);
    void setSystemAudioEnabled(bool enabled);
    void setLocalPlaybackEnabled(bool enabled);

signals:
    void pauseToggled(bool paused);
    void annotationToggled(bool enabled);
    void micMuteToggled(bool muted);
    void systemAudioToggled(bool enabled);
    void localPlaybackToggled(bool enabled);
    void backRequested();
    void stopRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void refreshTexts();

    QPushButton* m_pauseButton{nullptr};
    QPushButton* m_annotationButton{nullptr};
    QPushButton* m_micButton{nullptr};
    QPushButton* m_systemAudioButton{nullptr};
    QCheckBox* m_localPlaybackCheck{nullptr};
    QPushButton* m_backButton{nullptr};
    QPushButton* m_stopButton{nullptr};

    QPoint m_dragOffset;
    bool m_paused{false};
    bool m_annotationEnabled{false};
    bool m_micMuted{false};
    bool m_systemAudioEnabled{false};
};
