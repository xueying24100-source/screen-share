#include "annotationwindow.h"

#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

// ══════════════════════════════════════════════════════════════════════════════
// FloatingToolbar – a draggable, always-on-top toolbar widget
// ══════════════════════════════════════════════════════════════════════════════

class FloatingToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingToolbar(QWidget* parent = nullptr);

    void toggleTextMode();

    // Emits when any state changes that the overlay needs to react to
signals:
    void colorSelected(const QColor& color);
    void widthChanged(float width);
    void eraserToggled(bool on);
    void textModeChanged(bool on);
    void fontSizeChanged(int pointSize);
    void undoRequested();
    void redoRequested();
    void clearRequested();
    void exitRequested();
    void geometryChanged();   // emitted after a drag move

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPoint  m_dragStart;
    bool    m_dragging = false;
    bool    m_eraserOn = false;
    bool    m_textOn   = false;
    QPushButton* m_eraserButton = nullptr;
    QPushButton* m_textButton   = nullptr;

    // Helper to mark which color button is "active"
    QList<QPushButton*> m_colorButtons;
    void highlightColorButton(QPushButton* selected);
    void setTextButtonHighlight(bool on);
};

FloatingToolbar::FloatingToolbar(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral(
        "QWidget#FloatingToolbar { background: rgba(30,30,30,200); border-radius: 10px; }"
        "QPushButton { color: white; background: rgba(60,60,60,220); border: none;"
        "              border-radius: 6px; padding: 4px 8px; font-size: 14px; }"
        "QPushButton:hover { background: rgba(90,90,90,220); }"
        "QPushButton:pressed { background: rgba(40,40,40,220); }"
        "QComboBox { color: white; background: rgba(60,60,60,220); border: none;"
        "            border-radius: 6px; padding: 3px 6px; font-size: 13px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: rgba(40,40,40,240); color: white; }"
    ));
    setObjectName(QStringLiteral("FloatingToolbar"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);

    // ── Color buttons ──────────────────────────────────────────────────────
    struct ColorDef { const char* hex; QColor color; };
    const ColorDef colors[] = {
        { "#ff2d2d", Qt::red   },
        { "#2f7dff", Qt::blue  },
        { "#ffd21f", Qt::yellow},
        { "#2ecc71", Qt::green }
    };

    for (auto& cd : colors) {
        auto* btn = new QPushButton(this);
        btn->setFixedSize(26, 26);
        btn->setStyleSheet(QString::fromLatin1(
            "background-color: %1; border: 2px solid transparent;"
            "border-radius: 13px;").arg(QLatin1String(cd.hex)));
        const QColor c = cd.color;
        connect(btn, &QPushButton::clicked, this, [this, btn, c]() {
            highlightColorButton(btn);
            // Selecting a color switches off eraser and text modes
            m_eraserOn = false;
            m_textOn   = false;
            if (m_eraserButton)
                m_eraserButton->setText(QStringLiteral("✏️"));
            setTextButtonHighlight(false);
            emit colorSelected(c);
            emit eraserToggled(false);
            emit textModeChanged(false);
        });
        layout->addWidget(btn);
        m_colorButtons.append(btn);
    }

    // Highlight the first (red) button by default
    if (!m_colorButtons.isEmpty())
        highlightColorButton(m_colorButtons.first());

    // ── Width slider ───────────────────────────────────────────────────────
    auto* widthSlider = new QSlider(Qt::Horizontal, this);
    widthSlider->setRange(1, 20);
    widthSlider->setValue(3);
    widthSlider->setFixedWidth(110);
    widthSlider->setToolTip(QStringLiteral("笔宽"));
    connect(widthSlider, &QSlider::valueChanged, this, [this](int v) {
        emit widthChanged(static_cast<float>(v));
    });
    layout->addWidget(widthSlider);

    // ── Separator ──────────────────────────────────────────────────────────
    auto* sep = new QLabel(QStringLiteral("|"), this);
    sep->setStyleSheet(QStringLiteral("color: rgba(255,255,255,80);"));
    layout->addWidget(sep);

    // ── Eraser toggle ──────────────────────────────────────────────────────
    m_eraserButton = new QPushButton(QStringLiteral("✏️"), this);
    m_eraserButton->setFixedSize(34, 28);
    m_eraserButton->setToolTip(QStringLiteral("画笔 / 橡皮擦"));
    connect(m_eraserButton, &QPushButton::clicked, this, [this]() {
        m_eraserOn = !m_eraserOn;
        m_eraserButton->setText(m_eraserOn ? QStringLiteral("🧹") : QStringLiteral("✏️"));
        if (m_eraserOn) {
            // Turn off text mode when eraser is activated
            m_textOn = false;
            setTextButtonHighlight(false);
            emit textModeChanged(false);
        }
        emit eraserToggled(m_eraserOn);
    });
    layout->addWidget(m_eraserButton);

    // ── Text (T) tool ──────────────────────────────────────────────────────
    m_textButton = new QPushButton(QStringLiteral("T"), this);
    m_textButton->setFixedSize(28, 28);
    m_textButton->setToolTip(QStringLiteral("文字标注 (T)"));
    connect(m_textButton, &QPushButton::clicked, this, [this]() {
        m_textOn = !m_textOn;
        setTextButtonHighlight(m_textOn);
        if (m_textOn) {
            // Turn off eraser when text mode is activated
            m_eraserOn = false;
            if (m_eraserButton)
                m_eraserButton->setText(QStringLiteral("✏️"));
            emit eraserToggled(false);
        }
        emit textModeChanged(m_textOn);
    });
    layout->addWidget(m_textButton);

    // ── Font size combo ────────────────────────────────────────────────────
    auto* fontCombo = new QComboBox(this);
    fontCombo->addItem(QStringLiteral("12"), 12);
    fontCombo->addItem(QStringLiteral("16"), 16);
    fontCombo->addItem(QStringLiteral("20"), 20);
    fontCombo->setCurrentIndex(1);  // default 16
    fontCombo->setFixedWidth(52);
    fontCombo->setToolTip(QStringLiteral("字号 (pt)"));
    connect(fontCombo, &QComboBox::currentIndexChanged, this, [this, fontCombo](int idx) {
        emit fontSizeChanged(fontCombo->itemData(idx).toInt());
    });
    layout->addWidget(fontCombo);

    // ── Undo / Redo ────────────────────────────────────────────────────────
    auto* undoBtn = new QPushButton(QStringLiteral("↩"), this);
    undoBtn->setFixedSize(28, 28);
    undoBtn->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
    connect(undoBtn, &QPushButton::clicked, this, &FloatingToolbar::undoRequested);
    layout->addWidget(undoBtn);

    auto* redoBtn = new QPushButton(QStringLiteral("↪"), this);
    redoBtn->setFixedSize(28, 28);
    redoBtn->setToolTip(QStringLiteral("重做 (Ctrl+Shift+Z)"));
    connect(redoBtn, &QPushButton::clicked, this, &FloatingToolbar::redoRequested);
    layout->addWidget(redoBtn);

    // ── Clear ──────────────────────────────────────────────────────────────
    auto* clearBtn = new QPushButton(QStringLiteral("清空"), this);
    clearBtn->setToolTip(QStringLiteral("清空 (C)"));
    connect(clearBtn, &QPushButton::clicked, this, &FloatingToolbar::clearRequested);
    layout->addWidget(clearBtn);

    // ── Exit ───────────────────────────────────────────────────────────────
    auto* exitBtn = new QPushButton(QStringLiteral("退出"), this);
    exitBtn->setToolTip(QStringLiteral("退出 (Esc)"));
    connect(exitBtn, &QPushButton::clicked, this, &FloatingToolbar::exitRequested);
    layout->addWidget(exitBtn);

    adjustSize();
}

void FloatingToolbar::toggleTextMode()
{
    // Programmatically simulate the T button click
    m_textButton->click();
}

void FloatingToolbar::highlightColorButton(QPushButton* selected)
{
    for (QPushButton* btn : m_colorButtons) {
        QString s = btn->styleSheet();
        if (btn == selected) {
            s.replace(QStringLiteral("border: 2px solid transparent"),
                      QStringLiteral("border: 2px solid white"));
        } else {
            s.replace(QStringLiteral("border: 2px solid white"),
                      QStringLiteral("border: 2px solid transparent"));
        }
        btn->setStyleSheet(s);
    }
}

void FloatingToolbar::setTextButtonHighlight(bool on)
{
    if (!m_textButton) return;
    if (on) {
        m_textButton->setStyleSheet(
            QStringLiteral("background: rgba(255,255,255,60); border: 1px solid white;"
                           " border-radius: 6px; color: white; font-size: 14px; font-weight: bold;"));
    } else {
        m_textButton->setStyleSheet(QString()); // revert to global stylesheet
    }
}

void FloatingToolbar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStart = event->pos();
        m_dragging = true;
    }
    QWidget::mousePressEvent(event);
}

void FloatingToolbar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPos = mapToParent(event->pos()) - m_dragStart;
        move(newPos);
        emit geometryChanged();
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingToolbar::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

// ══════════════════════════════════════════════════════════════════════════════
// AnnotationWindow
// ══════════════════════════════════════════════════════════════════════════════

AnnotationWindow::AnnotationWindow(QWidget* parent, const QRect& targetGlobalRect)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // ── Overlay ───────────────────────────────────────────────────────────
    m_overlay = new AnnotationOverlay(this);

    // If a target rectangle is provided, the annotation layer is limited to that
    // area. This is used by the whiteboard so strokes cannot be drawn outside the
    // large preview region. Otherwise it stays fullscreen for desktop/window share.
    if (targetGlobalRect.isValid()) {
        setGeometry(targetGlobalRect);
        show();
    } else {
        showFullScreen();
    }

    m_overlay->setGeometry(rect());
    m_overlay->raise();

    // ── Floating toolbar (child widget, rendered inside AnnotationWindow) ─────
    auto* toolbar = new FloatingToolbar(this);
    m_toolbar = toolbar;
    toolbar->move(20, 20);
    toolbar->show();
    toolbar->raise();

    // Update the exclude rect so overlay ignores the toolbar area
    auto updateExclude = [this, toolbar]() {
        // Both toolbar and overlay are children of this; use toolbar->geometry() directly
        m_overlay->setToolbarExcludeRect(toolbar->geometry());
    };
    updateExclude();
    connect(toolbar, &FloatingToolbar::geometryChanged, this, updateExclude);

    // ── Wire toolbar signals to overlay slots ─────────────────────────────
    connect(toolbar, &FloatingToolbar::colorSelected,
            m_overlay, &AnnotationOverlay::setPenColor);
    connect(toolbar, &FloatingToolbar::widthChanged,
            m_overlay, &AnnotationOverlay::setPenWidth);
    connect(toolbar, &FloatingToolbar::eraserToggled,
            m_overlay, &AnnotationOverlay::setEraserMode);
    connect(toolbar, &FloatingToolbar::textModeChanged,
            m_overlay, &AnnotationOverlay::setTextMode);
    connect(toolbar, &FloatingToolbar::fontSizeChanged,
            m_overlay, &AnnotationOverlay::setFontSize);
    connect(toolbar, &FloatingToolbar::undoRequested,
            m_overlay, &AnnotationOverlay::undo);
    connect(toolbar, &FloatingToolbar::redoRequested,
            m_overlay, &AnnotationOverlay::redo);
    connect(toolbar, &FloatingToolbar::clearRequested,
            m_overlay, &AnnotationOverlay::clearAll);
    connect(toolbar, &FloatingToolbar::exitRequested, this, [this]() {
        emit closed();
        close();
    });

    // ── Forward stroke/text packets to the network layer ─────────────────
    connect(m_overlay, &AnnotationOverlay::strokePacketReady,
            this, &AnnotationWindow::strokePacketReady);
    connect(m_overlay, &AnnotationOverlay::textAnnotationCreated,
            this, &AnnotationWindow::textAnnotationCreated);
    connect(m_overlay, &AnnotationOverlay::contentChanged,
            this, &AnnotationWindow::contentChanged);

    // Debug / legacy
    connect(m_overlay, &AnnotationOverlay::strokeFinished, this, [](const Stroke& s) {
        qDebug() << "[strokeFinished] points:" << s.points.size();
    });
}

QPixmap AnnotationWindow::annotationPixmap() const
{
    return m_overlay ? m_overlay->toPixmap() : QPixmap();
}

bool AnnotationWindow::hasAnnotationContent() const
{
    return m_overlay && m_overlay->hasContent();
}

void AnnotationWindow::setAnnotationGeometry(const QRect& targetGlobalRect)
{
    if (!targetGlobalRect.isValid())
        return;

    setGeometry(targetGlobalRect);
    if (m_overlay) {
        m_overlay->setGeometry(rect());
        m_overlay->raise();
    }
    if (m_toolbar)
        m_toolbar->raise();
}

void AnnotationWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_overlay) {
        m_overlay->setGeometry(rect());
        m_overlay->raise();
    }
    if (m_toolbar)
        m_toolbar->raise();
}

void AnnotationWindow::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();

    if (event->key() == Qt::Key_Escape) {
        // If a text editor is active, Esc cancels input rather than closing the window
        if (m_overlay->isEditingText()) {
            m_overlay->cancelTextInput();
            return;
        }
        emit closed();
        close();
        return;
    }
    if (mods & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z) {
            if (mods & Qt::ShiftModifier)
                m_overlay->redo();
            else
                m_overlay->undo();
            return;
        }
        if (event->key() == Qt::Key_Y) {
            m_overlay->redo();
            return;
        }
    }
    if (!(mods & Qt::ControlModifier)) {
        if (event->key() == Qt::Key_T) {
            // Toggle text tool via toolbar so button state stays in sync
            if (auto* tb = qobject_cast<FloatingToolbar*>(m_toolbar))
                tb->toggleTextMode();
            return;
        }
        if (event->key() == Qt::Key_E) {
            // Toggle eraser – delegate to toolbar so its button state stays in sync
            if (auto* tb = qobject_cast<FloatingToolbar*>(m_toolbar))
                emit tb->eraserToggled(!m_overlay->property("eraserActive").toBool());
            else
                m_overlay->setEraserMode(true);
            return;
        }
        if (event->key() == Qt::Key_C) {
            m_overlay->clearAll();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

#include "annotationwindow.moc"
