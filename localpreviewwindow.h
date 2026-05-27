#pragma once

#include <QImage>
#include <QQueue>
#include <QString>
#include <QWidget>

#include "screencapturer.h"

class QLabel;
class QResizeEvent;

class LocalPreviewWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LocalPreviewWindow(QWidget* parent = nullptr);

    void updateFrame(const QImage& frame);
    void updateMetadata(const CaptureFrameMetadata& meta);
    void showError(const QString& error);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void refreshPreviewPixmap();
    void refreshStatusText();

    QLabel* m_previewLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_errorLabel{nullptr};

    QImage m_lastFrame;
    QString m_backendName{QStringLiteral("-")};
    QSize m_sourceSize;
    QSize m_displaySize;
    qint64 m_frameIndex{0};
    int m_fps{0};
    QQueue<qint64> m_frameTimesMs;
};
