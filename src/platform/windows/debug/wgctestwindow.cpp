#include "wgctestwindow.h"

#include "sourceenumerator.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPixmap>

WgcTestWindow::WgcTestWindow(QWidget* parent)
    : QWidget(parent)
    , m_capturer(new ScreenCapturer(this))
{
    m_windowCombo = new QComboBox(this);
    m_refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    m_startButton = new QPushButton(QStringLiteral("开始采集"), this);
    m_stopButton = new QPushButton(QStringLiteral("停止采集"), this);
    m_previewLabel = new QLabel(this);
    m_statusLabel = new QLabel(this);

    m_previewLabel->setFixedSize(960, 540);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(QStringLiteral("background:#111;color:#999;"));
    m_previewLabel->setText(QStringLiteral("暂无预览"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(m_windowCombo, 1);
    topRow->addWidget(m_refreshButton);
    topRow->addWidget(m_startButton);
    topRow->addWidget(m_stopButton);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topRow);
    mainLayout->addWidget(m_previewLabel, 0, Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    connect(m_refreshButton, &QPushButton::clicked, this, &WgcTestWindow::refreshWindows);
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (m_windowCombo->count() <= 0) {
            return;
        }
        const quintptr handle = static_cast<quintptr>(m_windowCombo->currentData().toULongLong());
        m_frameTimesMs.clear();
        m_fps = 0;
        m_capturer->startWindow(handle, 30);
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        m_capturer->stop();
        m_frameTimesMs.clear();
        m_fps = 0;
        updateStatusText();
    });

    connect(m_capturer, &ScreenCapturer::frameCaptured, this, [this](const QImage& frame) {
        if (frame.isNull()) {
            return;
        }
        m_previewLabel->setPixmap(QPixmap::fromImage(frame).scaled(
            960, 540, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_frameTimesMs.enqueue(now);
        while (!m_frameTimesMs.isEmpty() && (now - m_frameTimesMs.head()) > 1000) {
            m_frameTimesMs.dequeue();
        }
        m_fps = m_frameTimesMs.size();
        updateStatusText();
    });

    connect(m_capturer, &ScreenCapturer::frameMetadataChanged, this, [this](const CaptureFrameMetadata& meta) {
        m_backendName = meta.backendName;
        m_sourceSize = meta.sourceSize;
        m_frameIndex = meta.frameIndex;
        updateStatusText();
    });

    connect(m_capturer, &ScreenCapturer::captureError, this, [this](const QString& err) {
        m_statusLabel->setText(err);
    });

    refreshWindows();
    updateStatusText();
}

void WgcTestWindow::refreshWindows()
{
    m_windowCombo->clear();
    const QList<WindowInfo> windows = SourceEnumerator::enumerateWindows();
    for (const WindowInfo& info : windows) {
        m_windowCombo->addItem(info.title, QVariant::fromValue<qulonglong>(info.handle));
    }
}

void WgcTestWindow::updateStatusText()
{
    m_statusLabel->setText(
        QStringLiteral("Backend: %1 | %2×%3 | frame #%4 | %5 fps")
            .arg(m_backendName)
            .arg(m_sourceSize.width())
            .arg(m_sourceSize.height())
            .arg(m_frameIndex)
            .arg(m_fps));
}
