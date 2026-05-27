#include "ToolButton.h"

#include <QLabel>
#include <QVBoxLayout>

ToolButton::ToolButton(const QString &iconText, const QString &label,
                       bool checkable, QWidget *parent)
    : QPushButton(parent)
{
    setCheckable(checkable);
    setupUI(iconText, label);
}

void ToolButton::setActive(bool active)
{
    m_active = active;
    updateStyle();
}

bool ToolButton::isActive() const
{
    return m_active;
}

void ToolButton::setupUI(const QString &iconText, const QString &label)
{
    setFixedSize(72, 64);
    setCursor(Qt::PointingHandCursor);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 6, 4, 4);
    layout->setSpacing(2);

    QLabel *iconLabel = new QLabel(iconText, this);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 20px; background: transparent;");
    iconLabel->setAttribute(Qt::WA_TranslucentBackground);

    QLabel *textLabel = new QLabel(label, this);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setStyleSheet("font-size: 11px; color: #cccccc; background: transparent;");
    textLabel->setAttribute(Qt::WA_TranslucentBackground);

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel);

    updateStyle();
}

void ToolButton::updateStyle()
{
    QString base = "ToolButton { background: #3a3a3a; border: none; border-radius: 8px; }";
    QString hover = "ToolButton:hover { background: #4a4a4a; }";
    QString pressed = "ToolButton:pressed { background: #555555; }";
    QString active = m_active
        ? "ToolButton { background: #2d5aa0; }"
        : "";

    setStyleSheet(base + hover + pressed + active);
}
