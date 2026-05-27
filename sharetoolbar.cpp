#include "sharetoolbar.h"

#include <QEnterEvent>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>

ShareToolbar::ShareToolbar(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setObjectName(QStringLiteral("shareToolbarRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMouseTracking(true);
    setStyleSheet(QStringLiteral(
        "QWidget#shareToolbarRoot { background: #202430; border-radius: 10px; }"
        "QPushButton { color:#fff; background:transparent; border:none; padding:8px 10px; }"
        "QPushButton:hover { background:rgba(255,255,255,30); border-radius:6px; }"
        "QCheckBox { color:#fff; spacing:6px; padding:8px 8px; }"
        "QCheckBox::indicator { width:14px; height:14px; }"
        "QCheckBox::indicator:unchecked { border:1px solid #99a1b3; background:transparent; border-radius:3px; }"
        "QCheckBox::indicator:checked { border:1px solid #4da3ff; background:#4da3ff; border-radius:3px; }"
        "QPushButton#stopButton { color:#FF6B6B; }"));

    m_pauseButton = new QPushButton(this);
    m_annotationButton = new QPushButton(this);
    m_micButton = new QPushButton(this);
    m_systemAudioButton = new QPushButton(this);
    m_localPlaybackCheck = new QCheckBox(QStringLiteral("🎧 本地回放"), this);
    m_localPlaybackCheck->setToolTip(QStringLiteral("仅用于本地调试，外放会产生回声啸叫，请佩戴耳机"));
    m_localPlaybackCheck->setChecked(false);
    m_backButton = new QPushButton(QStringLiteral("⛶ 回到主窗口"), this);
    m_stopButton = new QPushButton(QStringLiteral("⛔ 结束共享"), this);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    layout->addWidget(m_pauseButton);
    layout->addWidget(m_annotationButton);
    layout->addWidget(m_micButton);
    layout->addWidget(m_systemAudioButton);
    layout->addWidget(m_localPlaybackCheck);
    layout->addWidget(m_backButton);
    layout->addWidget(m_stopButton);

    connect(m_pauseButton, &QPushButton::clicked, this, [this]() {
        m_paused = !m_paused;
        refreshTexts();
        emit pauseToggled(m_paused);
    });
    connect(m_annotationButton, &QPushButton::clicked, this, [this]() {
        m_annotationEnabled = !m_annotationEnabled;
        refreshTexts();
        emit annotationToggled(m_annotationEnabled);
    });
    connect(m_micButton, &QPushButton::clicked, this, [this]() {
        m_micMuted = !m_micMuted;
        refreshTexts();
        emit micMuteToggled(m_micMuted);
    });
    connect(m_systemAudioButton, &QPushButton::clicked, this, [this]() {
        m_systemAudioEnabled = !m_systemAudioEnabled;
        refreshTexts();
        emit systemAudioToggled(m_systemAudioEnabled);
    });
    connect(m_localPlaybackCheck, &QCheckBox::toggled, this, &ShareToolbar::localPlaybackToggled);
    connect(m_backButton, &QPushButton::clicked, this, &ShareToolbar::backRequested);
    connect(m_stopButton, &QPushButton::clicked, this, &ShareToolbar::stopRequested);

    refreshTexts();
}

void ShareToolbar::setPaused(bool paused)
{
    m_paused = paused;
    refreshTexts();
}

void ShareToolbar::setAnnotationEnabled(bool enabled)
{
    m_annotationEnabled = enabled;
    refreshTexts();
}

void ShareToolbar::setMicMuted(bool muted)
{
    m_micMuted = muted;
    refreshTexts();
}

void ShareToolbar::setSystemAudioEnabled(bool enabled)
{
    m_systemAudioEnabled = enabled;
    refreshTexts();
}

void ShareToolbar::setLocalPlaybackEnabled(bool enabled)
{
    if (!m_localPlaybackCheck) {
        return;
    }
    const QSignalBlocker blocker(m_localPlaybackCheck);
    m_localPlaybackCheck->setChecked(enabled);
}

void ShareToolbar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void ShareToolbar::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
    QWidget::mouseMoveEvent(event);
}

void ShareToolbar::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
}

void ShareToolbar::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
}

void ShareToolbar::refreshTexts()
{
    m_pauseButton->setText(m_paused ? QStringLiteral("▶ 继续共享") : QStringLiteral("⏸ 暂停共享"));
    m_annotationButton->setText(m_annotationEnabled ? QStringLiteral("✏️ 批注:开") : QStringLiteral("✏️ 批注:关"));
    m_micButton->setText(m_micMuted ? QStringLiteral("🔇 麦克风:静音") : QStringLiteral("🎤 麦克风:开启"));
    m_systemAudioButton->setText(m_systemAudioEnabled ? QStringLiteral("🔊 共享声音") : QStringLiteral("🔈 共享声音:关"));
}
