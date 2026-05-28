#pragma once

#include <QWidget>
#include <QQueue>
#include "media/capture/screen/screencapturer.h"

class QComboBox;
class QLabel;
class QPushButton;

class WgcTestWindow : public QWidget
{
    Q_OBJECT

public:
    explicit WgcTestWindow(QWidget* parent = nullptr);

private:
    void refreshWindows();
    void updateStatusText();

    ScreenCapturer* m_capturer{nullptr};
    QComboBox*      m_windowCombo{nullptr};
    QPushButton*    m_refreshButton{nullptr};
    QPushButton*    m_startButton{nullptr};
    QPushButton*    m_stopButton{nullptr};
    QLabel*         m_previewLabel{nullptr};
    QLabel*         m_statusLabel{nullptr};

    QString m_backendName{QStringLiteral("-")};
    QSize   m_sourceSize;
    qint64  m_frameIndex{0};
    int     m_fps{0};
    QQueue<qint64> m_frameTimesMs;
};
