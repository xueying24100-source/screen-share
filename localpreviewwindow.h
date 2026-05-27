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

public slots:
    void updateMicLevel(double dbfs);
    void updateSystemLevel(double dbfs);
    void setDeviceLabels(const QString& micName, const QString& outName);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void refreshPreviewPixmap();
    void refreshStatusText();

    QLabel* m_previewLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_errorLabel{nullptr};
    QLabel* m_micDbLabel{nullptr};
    QLabel* m_systemDbLabel{nullptr};
    QLabel* m_micDeviceLabel{nullptr};
    QLabel* m_systemDeviceLabel{nullptr};
    QWidget* m_micVolumeBar{nullptr};
    QWidget* m_systemVolumeBar{nullptr};

    QImage m_lastFrame;
    QString m_backendName{QStringLiteral("-")};
    QSize m_sourceSize;
    QSize m_displaySize;
    qint64 m_frameIndex{0};
    int m_fps{0};
    QQueue<qint64> m_frameTimesMs;
    double m_smoothedMicDb{-60.0};
    double m_smoothedSystemDb{-60.0};
};
