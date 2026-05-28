#include "ui/preview/localpreviewwindow.h"

#include <QDateTime>
#include <QPainter>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cmath>

namespace {

class VolumeBar : public QWidget
{
public:
    explicit VolumeBar(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(9);
        setMinimumWidth(120);
    }

    void setLevelDbFs(double db)
    {
        m_db = qBound(-60.0, db, 0.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF frameRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QColor(75, 83, 98));
        painter.setBrush(QColor(26, 30, 40));
        painter.drawRoundedRect(frameRect, 4.0, 4.0);

        const double ratio = (m_db + 60.0) / 60.0;
        const double fillWidth = qMax(0.0, (frameRect.width() - 2.0) * ratio);
        if (fillWidth <= 0.0) {
            return;
        }

        QColor color(56, 197, 117);
        if (m_db >= -6.0) {
            color = QColor(255, 82, 82);
        } else if (m_db >= -20.0) {
            color = QColor(244, 196, 48);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(frameRect.left() + 1.0, frameRect.top() + 1.0, fillWidth, frameRect.height() - 2.0), 3.0, 3.0);
    }

private:
    double m_db{-60.0};
};

double clampDb(double db)
{
    if (!std::isfinite(db)) {
        return -60.0;
    }
    return qBound(-60.0, db, 0.0);
}

QString dbLabelText(double db)
{
    if (db <= -89.0) {
        return QStringLiteral("-inf dBFS");
    }
    return QStringLiteral("%1 dBFS").arg(qRound(db));
}

} // namespace

LocalPreviewWindow::LocalPreviewWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("本地预览"));
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("localPreviewWindow"));
    resize(960, 540);
    setMinimumSize(320, 180);
    setStyleSheet(QStringLiteral(
        "QWidget#localPreviewWindow { background:#10131a; border:1px solid #2d3443; border-radius:10px; }"
        "QLabel { color:#d6d8de; }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    m_previewLabel = new QLabel(QStringLiteral("等待共享画面..."), this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(true);
    m_previewLabel->setMinimumSize(0, 0);
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

    auto* volumeRootLayout = new QVBoxLayout();
    volumeRootLayout->setSpacing(4);
    volumeRootLayout->setContentsMargins(0, 2, 0, 0);

    auto* micRow = new QHBoxLayout();
    micRow->setSpacing(8);
    auto* micTitle = new QLabel(QStringLiteral("Mic"), this);
    micTitle->setFixedWidth(52);
    m_micVolumeBar = new VolumeBar(this);
    m_micDbLabel = new QLabel(QStringLiteral("-inf dBFS"), this);
    m_micDbLabel->setFixedWidth(74);
    m_micDeviceLabel = new QLabel(QStringLiteral("(默认输入设备)"), this);
    m_micDeviceLabel->setStyleSheet(QStringLiteral("color:#9aa4b2;font-size:12px;"));
    micRow->addWidget(micTitle);
    micRow->addWidget(m_micVolumeBar, 1);
    micRow->addWidget(m_micDbLabel);
    micRow->addWidget(m_micDeviceLabel);

    auto* systemRow = new QHBoxLayout();
    systemRow->setSpacing(8);
    auto* systemTitle = new QLabel(QStringLiteral("System"), this);
    systemTitle->setFixedWidth(52);
    m_systemVolumeBar = new VolumeBar(this);
    m_systemDbLabel = new QLabel(QStringLiteral("-inf dBFS"), this);
    m_systemDbLabel->setFixedWidth(74);
    m_systemDeviceLabel = new QLabel(QStringLiteral("(默认输出设备)"), this);
    m_systemDeviceLabel->setStyleSheet(QStringLiteral("color:#9aa4b2;font-size:12px;"));
    systemRow->addWidget(systemTitle);
    systemRow->addWidget(m_systemVolumeBar, 1);
    systemRow->addWidget(m_systemDbLabel);
    systemRow->addWidget(m_systemDeviceLabel);

    volumeRootLayout->addLayout(micRow);
    volumeRootLayout->addLayout(systemRow);
    layout->addLayout(volumeRootLayout);
    layout->addWidget(m_errorLabel);

    updateMicLevel(-90.0);
    updateSystemLevel(-90.0);
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
    // With setScaledContents(true), QLabel re-scales automatically on resize.
}

void LocalPreviewWindow::updateMicLevel(double dbfs)
{
    m_smoothedMicDb = (m_smoothedMicDb * 0.6) + (dbfs * 0.4);
    const double clamped = clampDb(m_smoothedMicDb);
    if (m_micVolumeBar) {
        static_cast<VolumeBar*>(m_micVolumeBar)->setLevelDbFs(clamped);
    }
    if (m_micDbLabel) {
        m_micDbLabel->setText(dbLabelText(m_smoothedMicDb));
    }
}

void LocalPreviewWindow::updateSystemLevel(double dbfs)
{
    m_smoothedSystemDb = (m_smoothedSystemDb * 0.6) + (dbfs * 0.4);
    const double clamped = clampDb(m_smoothedSystemDb);
    if (m_systemVolumeBar) {
        static_cast<VolumeBar*>(m_systemVolumeBar)->setLevelDbFs(clamped);
    }
    if (m_systemDbLabel) {
        m_systemDbLabel->setText(dbLabelText(m_smoothedSystemDb));
    }
}

void LocalPreviewWindow::setDeviceLabels(const QString& micName, const QString& outName)
{
    if (m_micDeviceLabel) {
        m_micDeviceLabel->setText(QStringLiteral("(%1)").arg(micName.isEmpty() ? QStringLiteral("默认输入设备") : micName));
    }
    if (m_systemDeviceLabel) {
        m_systemDeviceLabel->setText(QStringLiteral("(%1)").arg(outName.isEmpty() ? QStringLiteral("默认输出设备") : outName));
    }
}

void LocalPreviewWindow::refreshPreviewPixmap()
{
    if (m_lastFrame.isNull() || !m_previewLabel) {
        return;
    }

    // Let Qt/GPU handle scaling via QLabel::setScaledContents(true).
    // Avoid CPU-side scaled() to keep the main thread free.
    const QPixmap pixmap = QPixmap::fromImage(m_lastFrame);
    m_displaySize = pixmap.size();
    m_previewLabel->setPixmap(pixmap);
    refreshStatusText();
}

void LocalPreviewWindow::refreshStatusText()
{
    const QString displaySizeText = m_displaySize.isValid()
        ? QStringLiteral("%1×%2").arg(m_displaySize.width()).arg(m_displaySize.height())
        : QStringLiteral("-");
    m_statusLabel->setText(
        QStringLiteral("Backend: %1 | %2×%3 | frame #%4 | %5 fps | 显示 %6 | 可拖动边角调整预览大小")
            .arg(m_backendName)
            .arg(m_sourceSize.width())
            .arg(m_sourceSize.height())
            .arg(m_frameIndex)
            .arg(m_fps)
            .arg(displaySizeText));
}
