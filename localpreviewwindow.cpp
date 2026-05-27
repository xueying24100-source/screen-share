#include "localpreviewwindow.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QVBoxLayout>

LocalPreviewWindow::LocalPreviewWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("本地预览"));
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("localPreviewWindow"));
    resize(480, 270);
    setStyleSheet(QStringLiteral(
        "QWidget#localPreviewWindow { background:#10131a; border:1px solid #2d3443; border-radius:10px; }"
        "QLabel { color:#d6d8de; }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    m_previewLabel = new QLabel(QStringLiteral("等待共享画面..."), this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setMinimumSize(440, 210);
    m_previewLabel->setStyleSheet(QStringLiteral("background:#0b0d12;border-radius:6px;"));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#9aa4b2;font-size:12px;"));

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#ff6b6b;font-size:12px;"));
    m_errorLabel->hide();

    layout->addWidget(m_previewLabel, 1);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_errorLabel);

    refreshStatusText();
}

void LocalPreviewWindow::updateFrame(const QImage& frame)
{
    if (frame.isNull()) {
        return;
    }

    m_lastFrame = frame;
    refreshPreviewPixmap();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_frameTimesMs.enqueue(now);
    while (!m_frameTimesMs.isEmpty() && (now - m_frameTimesMs.head()) > 1000) {
        m_frameTimesMs.dequeue();
    }
    m_fps = m_frameTimesMs.size();
    m_errorLabel->clear();
    m_errorLabel->hide();
    refreshStatusText();
}

void LocalPreviewWindow::updateMetadata(const CaptureFrameMetadata& meta)
{
    m_backendName = meta.backendName;
    m_sourceSize = meta.sourceSize;
    m_frameIndex = meta.frameIndex;
    refreshStatusText();
}

void LocalPreviewWindow::showError(const QString& error)
{
    if (error.isEmpty()) {
        m_errorLabel->clear();
        m_errorLabel->hide();
        return;
    }

    m_errorLabel->setText(error);
    m_errorLabel->show();
}

void LocalPreviewWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshPreviewPixmap();
}

void LocalPreviewWindow::refreshPreviewPixmap()
{
    if (m_lastFrame.isNull() || !m_previewLabel) {
        return;
    }

    m_previewLabel->setPixmap(QPixmap::fromImage(m_lastFrame).scaled(
        m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void LocalPreviewWindow::refreshStatusText()
{
    m_statusLabel->setText(
        QStringLiteral("Backend: %1 | %2×%3 | frame #%4 | %5 fps")
            .arg(m_backendName)
            .arg(m_sourceSize.width())
            .arg(m_sourceSize.height())
            .arg(m_frameIndex)
            .arg(m_fps));
}
