#include "annotationwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDebug>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSysInfo>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

// ══════════════════════════════════════════════════════════════════════════════
// FloatingToolbar – 9 tools + color + width + undo/redo + clear + exit
// ══════════════════════════════════════════════════════════════════════════════

class FloatingToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingToolbar(QWidget* parent = nullptr);

    void setActiveTool(AnnotationTool tool);
    void setUndoRedoEnabled(bool canUndo, bool canRedo);

signals:
    void colorSelected(const QColor& color);
    void widthChanged(float width);
    void toolSelected(AnnotationTool tool);
    void fontSizeChanged(int pointSize);
    void undoRequested();
    void redoRequested();
    void clearRequested();
    void exitRequested();
    void geometryChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPoint  m_dragStart;
    bool    m_dragging = false;

    QList<QPushButton*>           m_colorButtons;
    QList<QPushButton*>           m_recentSlots;     // 4 buttons reused for recent picks
    QList<QColor>                 m_recentColors;    // most-recent-first, max 4
    QMap<AnnotationTool, QToolButton*> m_toolButtons;
    QPushButton*                  m_undoBtn = nullptr;
    QPushButton*                  m_redoBtn = nullptr;

    void highlightColorButton(QPushButton* selected);
    void pushRecentColor(const QColor& c);
    QToolButton* makeToolButton(const QString& text, const QString& tooltip, AnnotationTool tool);
};

FloatingToolbar::FloatingToolbar(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral(
        "QWidget#FloatingToolbar { background: rgba(30,30,30,210); border-radius: 10px; }"
        "QPushButton { color: white; background: rgba(60,60,60,220); border: none;"
        "              border-radius: 6px; padding: 4px 8px; font-size: 14px; }"
        "QPushButton:hover { background: rgba(90,90,90,220); }"
        "QPushButton:pressed { background: rgba(40,40,40,220); }"
        "QPushButton:disabled { color: rgba(255,255,255,80); background: rgba(60,60,60,140); }"
        "QToolButton { color: white; background: rgba(60,60,60,220); border: none;"
        "              border-radius: 6px; padding: 4px 6px; font-size: 14px; }"
        "QToolButton:hover { background: rgba(90,90,90,220); }"
        "QToolButton:checked { background: rgba(120,160,255,220); }"
        "QComboBox { color: white; background: rgba(60,60,60,220); border: none;"
        "            border-radius: 6px; padding: 3px 6px; font-size: 13px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: rgba(40,40,40,240); color: white; }"
    ));
    setObjectName(QStringLiteral("FloatingToolbar"));

    // Two-row layout to keep the bar compact.
    //   Row 1: colors  |  width slider  |  9 tools
    //   Row 2: font   undo redo  clear  exit
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(4);

    auto* row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(4);
    auto* row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(6);

    root->addLayout(row1);
    root->addLayout(row2);

    // ── Color buttons (row 1) ──────────────────────────────────────────
    struct ColorDef { const char* hex; QColor color; };
    const ColorDef colors[] = {
        { "#ff2d2d", Qt::red    },
        { "#2f7dff", Qt::blue   },
        { "#ffd21f", Qt::yellow },
        { "#2ecc71", Qt::green  },
    };
    for (auto& cd : colors) {
        auto* btn = new QPushButton(this);
        btn->setFixedSize(22, 22);
        btn->setStyleSheet(QString::fromLatin1(
            "background-color: %1; border: 2px solid transparent;"
            "border-radius: 11px;").arg(QLatin1String(cd.hex)));
        const QColor c = cd.color;
        connect(btn, &QPushButton::clicked, this, [this, btn, c]() {
            highlightColorButton(btn);
            emit colorSelected(c);
        });
        row1->addWidget(btn);
        m_colorButtons.append(btn);
    }
    if (!m_colorButtons.isEmpty())
        highlightColorButton(m_colorButtons.first());

    // ── Custom color picker (row 1) ────────────────────────────────────
    // Opens a QColorDialog and remembers the last pick on the button face.
    auto* customColorBtn = new QPushButton(this);
    customColorBtn->setFixedSize(22, 22);
    customColorBtn->setToolTip(QStringLiteral("自定义颜色"));
    auto applyCustomFace = [customColorBtn](const QColor& c) {
        // Conic-gradient look so it stands apart from the solid swatches.
        customColorBtn->setStyleSheet(QString::fromLatin1(
            "background: qconicalgradient(cx:0.5, cy:0.5, angle:0,"
            " stop:0 red, stop:0.16 yellow, stop:0.33 green,"
            " stop:0.5 cyan, stop:0.66 blue, stop:0.83 magenta, stop:1 red);"
            " border: 2px solid %1; border-radius: 11px;")
            .arg(c.isValid() ? c.name() : QStringLiteral("white")));
    };
    applyCustomFace(QColor());
    connect(customColorBtn, &QPushButton::clicked, this, [this, customColorBtn, applyCustomFace]() {
        QColor c = QColorDialog::getColor(Qt::yellow, this, QStringLiteral("选择颜色"),
                                          QColorDialog::ShowAlphaChannel);
        if (!c.isValid())
            return;
        // Deselect preset swatches so the user sees that "custom" is active.
        for (QPushButton* sw : m_colorButtons) {
            QString s = sw->styleSheet();
            s.replace(QStringLiteral("border: 2px solid white"),
                      QStringLiteral("border: 2px solid transparent"));
            sw->setStyleSheet(s);
        }
        applyCustomFace(c);
        pushRecentColor(c);
        emit colorSelected(c);
    });
    row1->addWidget(customColorBtn);

    // ── Recent custom colors: 4 slots (start hidden, light up as user picks) ──
    for (int i = 0; i < 4; ++i) {
        auto* slot = new QPushButton(this);
        slot->setFixedSize(18, 18);
        slot->setVisible(false);
        slot->setToolTip(QStringLiteral("最近使用"));
        connect(slot, &QPushButton::clicked, this, [this, i]() {
            if (i < m_recentColors.size())
                emit colorSelected(m_recentColors[i]);
        });
        row1->addWidget(slot);
        m_recentSlots.append(slot);
    }

    // ── Width slider (row 1) ───────────────────────────────────────────
    auto* widthSlider = new QSlider(Qt::Horizontal, this);
    widthSlider->setRange(1, 20);
    widthSlider->setValue(3);
    widthSlider->setFixedWidth(80);
    widthSlider->setToolTip(QStringLiteral("笔宽"));
    connect(widthSlider, &QSlider::valueChanged, this, [this](int v) {
        emit widthChanged(static_cast<float>(v));
    });
    row1->addWidget(widthSlider);

    auto* sep1 = new QLabel(QStringLiteral("|"), this);
    sep1->setStyleSheet(QStringLiteral("color: rgba(255,255,255,80);"));
    row1->addWidget(sep1);

    // ── Tools — 9 buttons (row 1) ──────────────────────────────────────
    // Plain Unicode glyphs (no emoji) render reliably across Windows fonts.
    auto* penBtn         = makeToolButton(QStringLiteral("✎"),  QStringLiteral("画笔 (P)"),    AnnotationTool::Pen);
    auto* eraserBtn      = makeToolButton(QStringLiteral("⌫"), QStringLiteral("橡皮 (E)"),    AnnotationTool::Eraser);
    auto* textBtn        = makeToolButton(QStringLiteral("T"),  QStringLiteral("文字 (T)"),    AnnotationTool::Text);
    auto* lineBtn        = makeToolButton(QStringLiteral("╱"),  QStringLiteral("直线 (L)"),    AnnotationTool::Line);
    auto* rectBtn        = makeToolButton(QStringLiteral("▢"),  QStringLiteral("矩形 (R)"),    AnnotationTool::Rect);
    auto* ellipseBtn     = makeToolButton(QStringLiteral("○"),  QStringLiteral("椭圆 (O)"),    AnnotationTool::Ellipse);
    auto* arrowBtn       = makeToolButton(QStringLiteral("→"),  QStringLiteral("箭头 (A)"),    AnnotationTool::Arrow);
    auto* highlighterBtn = makeToolButton(QStringLiteral("▮"), QStringLiteral("荧光笔 (H)"),  AnnotationTool::Highlighter);
    auto* laserBtn       = makeToolButton(QStringLiteral("⊙"),  QStringLiteral("激光笔 (K)"), AnnotationTool::Laser);
    for (QToolButton* b : { penBtn, eraserBtn, textBtn, lineBtn, rectBtn,
                            ellipseBtn, arrowBtn, highlighterBtn, laserBtn })
        row1->addWidget(b);

    row1->addStretch(1);

    // ── Font size combo (row 2) ────────────────────────────────────────
    auto* fontLabel = new QLabel(QStringLiteral("字号"), this);
    fontLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,180); font-size: 12px;"));
    row2->addWidget(fontLabel);

    auto* fontCombo = new QComboBox(this);
    fontCombo->addItem(QStringLiteral("12"), 12);
    fontCombo->addItem(QStringLiteral("16"), 16);
    fontCombo->addItem(QStringLiteral("20"), 20);
    fontCombo->addItem(QStringLiteral("24"), 24);
    fontCombo->setCurrentIndex(1);
    fontCombo->setFixedWidth(50);
    fontCombo->setToolTip(QStringLiteral("字号 (pt)"));
    connect(fontCombo, &QComboBox::currentIndexChanged, this, [this, fontCombo](int idx) {
        emit fontSizeChanged(fontCombo->itemData(idx).toInt());
    });
    row2->addWidget(fontCombo);

    auto* sep2 = new QLabel(QStringLiteral("|"), this);
    sep2->setStyleSheet(QStringLiteral("color: rgba(255,255,255,80);"));
    row2->addWidget(sep2);

    // ── Undo / Redo (row 2) ────────────────────────────────────────────
    m_undoBtn = new QPushButton(QStringLiteral("↩"), this);
    m_undoBtn->setFixedSize(28, 26);
    m_undoBtn->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
    m_undoBtn->setEnabled(false);
    connect(m_undoBtn, &QPushButton::clicked, this, &FloatingToolbar::undoRequested);
    row2->addWidget(m_undoBtn);

    m_redoBtn = new QPushButton(QStringLiteral("↪"), this);
    m_redoBtn->setFixedSize(28, 26);
    m_redoBtn->setToolTip(QStringLiteral("重做 (Ctrl+Shift+Z)"));
    m_redoBtn->setEnabled(false);
    connect(m_redoBtn, &QPushButton::clicked, this, &FloatingToolbar::redoRequested);
    row2->addWidget(m_redoBtn);

    // ── Clear (row 2) ──────────────────────────────────────────────────
    auto* clearBtn = new QPushButton(QStringLiteral("清空"), this);
    clearBtn->setFixedHeight(26);
    clearBtn->setToolTip(QStringLiteral("清空 (C)"));
    connect(clearBtn, &QPushButton::clicked, this, &FloatingToolbar::clearRequested);
    row2->addWidget(clearBtn);

    // ── Exit (row 2) ───────────────────────────────────────────────────
    auto* exitBtn = new QPushButton(QStringLiteral("退出"), this);
    exitBtn->setFixedHeight(26);
    exitBtn->setToolTip(QStringLiteral("退出 (Esc)"));
    connect(exitBtn, &QPushButton::clicked, this, &FloatingToolbar::exitRequested);
    row2->addWidget(exitBtn);

    row2->addStretch(1);

    setActiveTool(AnnotationTool::Pen);
    adjustSize();
}

QToolButton* FloatingToolbar::makeToolButton(const QString& text, const QString& tooltip, AnnotationTool tool)
{
    auto* btn = new QToolButton(this);
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setFixedSize(34, 30);
    btn->setCheckable(true);
    QFont f = btn->font();
    f.setPointSize(13);
    f.setBold(true);
    btn->setFont(f);
    connect(btn, &QToolButton::clicked, this, [this, tool]() {
        setActiveTool(tool);
        emit toolSelected(tool);
    });
    m_toolButtons.insert(tool, btn);
    return btn;
}

void FloatingToolbar::setActiveTool(AnnotationTool tool)
{
    for (auto it = m_toolButtons.constBegin(); it != m_toolButtons.constEnd(); ++it)
        it.value()->setChecked(it.key() == tool);
}

void FloatingToolbar::setUndoRedoEnabled(bool canUndo, bool canRedo)
{
    if (m_undoBtn) m_undoBtn->setEnabled(canUndo);
    if (m_redoBtn) m_redoBtn->setEnabled(canRedo);
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

void FloatingToolbar::pushRecentColor(const QColor& c)
{
    if (!c.isValid())
        return;
    // Move to front; dedup if already present
    m_recentColors.removeAll(c);
    m_recentColors.prepend(c);
    while (m_recentColors.size() > m_recentSlots.size())
        m_recentColors.removeLast();

    // Repaint the visible swatch slots
    for (int i = 0; i < m_recentSlots.size(); ++i) {
        QPushButton* slot = m_recentSlots[i];
        if (i < m_recentColors.size()) {
            slot->setStyleSheet(QString::fromLatin1(
                "background-color: %1; border: 1px solid rgba(255,255,255,140);"
                "border-radius: 9px;").arg(m_recentColors[i].name()));
            slot->setVisible(true);
        } else {
            slot->setVisible(false);
        }
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

        // Edge snap: pull to a parent edge when within snapDistance px.
        if (QWidget* p = parentWidget()) {
            constexpr int snapDistance = 16;
            const QRect parentRect = p->rect();
            const int rightEdge = parentRect.width() - width();
            const int bottomEdge = parentRect.height() - height();
            if (qAbs(newPos.x()) < snapDistance)               newPos.setX(0);
            else if (qAbs(rightEdge - newPos.x()) < snapDistance)  newPos.setX(rightEdge);
            if (qAbs(newPos.y()) < snapDistance)               newPos.setY(0);
            else if (qAbs(bottomEdge - newPos.y()) < snapDistance) newPos.setY(bottomEdge);
            // Keep inside the parent rect
            newPos.setX(qBound(0, newPos.x(), rightEdge));
            newPos.setY(qBound(0, newPos.y(), bottomEdge));
        }

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

quint32 AnnotationWindow::generateUserId()
{
    // Combine machine ID + PID into a 32-bit hash. ADR-007 only needs
    // uniqueness in a small LAN session; collision probability is acceptably low.
    QByteArray seed = QSysInfo::machineUniqueId();
    if (seed.isEmpty())
        seed = QSysInfo::machineHostName().toUtf8();
    seed += QByteArray::number(QCoreApplication::applicationPid());

    const QByteArray hash = QCryptographicHash::hash(seed, QCryptographicHash::Md5);
    quint32 id = 0;
    if (hash.size() >= 4) {
        id = (quint8(hash[0]) << 24) | (quint8(hash[1]) << 16)
           | (quint8(hash[2]) << 8)  | (quint8(hash[3]));
    }
    return id == 0 ? 1 : id; // 0 is reserved
}

AnnotationWindow::AnnotationWindow(QWidget* parent)
    : QWidget(parent)
    , m_userId(generateUserId())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    m_overlay = new AnnotationOverlay(m_userId, this);
    resize(800, 600);
    m_overlay->setGeometry(rect());
    m_overlay->raise();

    auto* toolbar = new FloatingToolbar(this);
    m_toolbar = toolbar;
    toolbar->move(20, 20);
    toolbar->show();
    toolbar->raise();

    auto updateExclude = [this, toolbar]() {
        m_overlay->setToolbarExcludeRect(toolbar->geometry());
    };
    updateExclude();
    connect(toolbar, &FloatingToolbar::geometryChanged, this, updateExclude);

    // Wire toolbar → overlay
    connect(toolbar, &FloatingToolbar::colorSelected,
            m_overlay, &AnnotationOverlay::setPenColor);
    connect(toolbar, &FloatingToolbar::widthChanged,
            m_overlay, &AnnotationOverlay::setPenWidth);
    connect(toolbar, &FloatingToolbar::toolSelected,
            m_overlay, &AnnotationOverlay::setTool);
    connect(toolbar, &FloatingToolbar::fontSizeChanged,
            m_overlay, &AnnotationOverlay::setFontSize);
    connect(toolbar, &FloatingToolbar::undoRequested,
            m_overlay, &AnnotationOverlay::undo);
    connect(toolbar, &FloatingToolbar::redoRequested,
            m_overlay, &AnnotationOverlay::redo);
    connect(toolbar, &FloatingToolbar::clearRequested,
            m_overlay, &AnnotationOverlay::clearAll);
    connect(toolbar, &FloatingToolbar::exitRequested,
            this, &AnnotationWindow::requestExit);

    // Wire overlay state → toolbar
    connect(m_overlay, &AnnotationOverlay::undoRedoChanged, this, [this, toolbar]() {
        toolbar->setUndoRedoEnabled(m_overlay->canUndo(), m_overlay->canRedo());
    });
    connect(m_overlay, &AnnotationOverlay::toolChanged, this, [toolbar](AnnotationTool tool) {
        toolbar->setActiveTool(tool);
    });

    // Forward overlay output to external listeners
    connect(m_overlay, &AnnotationOverlay::strokePacketReady,
            this, &AnnotationWindow::strokePacketReady);
    connect(m_overlay, &AnnotationOverlay::textAnnotationCreated,
            this, &AnnotationWindow::textAnnotationCreated);
    connect(m_overlay, &AnnotationOverlay::contentChanged,
            this, &AnnotationWindow::contentChanged);
    connect(m_overlay, &AnnotationOverlay::closeRequested,
            this, &AnnotationWindow::requestExit);

    connect(m_overlay, &AnnotationOverlay::strokeFinished, this, [](const Stroke& s) {
        qDebug() << "[strokeFinished] uid=" << s.userId
                 << " sid=" << s.strokeId
                 << " type=" << static_cast<int>(s.type)
                 << " pts=" << s.points.size();
    });
}

void AnnotationWindow::setTargetGeometry(const QRect& screenRect)
{
    setGeometry(screenRect);
    if (m_overlay)
        m_overlay->setGeometry(rect());
    raise();
    activateWindow();
}

void AnnotationWindow::requestExit()
{
    // If there are annotations, offer to copy them to clipboard before quitting.
    // Skip the prompt during in-progress drawing (Esc already cancels that path
    // inside the overlay) and when there is no content at all.
    if (m_overlay && m_overlay->hasRenderableContent()) {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("退出画笔"));
        box.setText(QStringLiteral("当前批注未保存，是否复制到剪贴板？"));
        QPushButton* btnCopy   = box.addButton(QStringLiteral("复制并退出"),
                                               QMessageBox::AcceptRole);
        QPushButton* btnDirect = box.addButton(QStringLiteral("直接退出"),
                                               QMessageBox::DestructiveRole);
        QPushButton* btnCancel = box.addButton(QStringLiteral("取消"),
                                               QMessageBox::RejectRole);
        box.setDefaultButton(btnCopy);
        box.exec();
        QAbstractButton* clicked = box.clickedButton();
        if (clicked == btnCancel)
            return;
        if (clicked == btnCopy)
            copyToClipboard();
        Q_UNUSED(btnDirect);
    }
    emitClosedOnce();
    close();
}

QImage AnnotationWindow::renderAnnotationsToImage(const QSize& targetSize) const
{
    return m_overlay ? m_overlay->renderAnnotationsToImage(targetSize) : QImage{};
}

bool AnnotationWindow::hasRenderableContent() const
{
    return m_overlay && m_overlay->hasRenderableContent();
}

void AnnotationWindow::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const int key = event->key();

    if (key == Qt::Key_Escape) {
        if (m_overlay->isEditingText()) {
            m_overlay->cancelTextInput();
            return;
        }
        requestExit();
        return;
    }

    // Undo/Redo
    if (mods & Qt::ControlModifier) {
        if (key == Qt::Key_Z) {
            if (mods & Qt::ShiftModifier) m_overlay->redo();
            else                          m_overlay->undo();
            return;
        }
        if (key == Qt::Key_Y) { m_overlay->redo(); return; }
        if (key == Qt::Key_S) { copyToClipboard(); return; }
        // Stress test trigger:
        //   Ctrl+G        → 1000 strokes
        //   Ctrl+Shift+G  → 5000 strokes
        // (F12 was the original binding but laptop Fn keys make it
        //  unreliable to press; Ctrl+G works everywhere.)
        if (key == Qt::Key_G) {
            const int n = (mods & Qt::ShiftModifier) ? 5000 : 1000;
            m_overlay->runStressTest(n);
            return;
        }
    }

    // Tool shortcuts (no modifier)
    if (!(mods & Qt::ControlModifier) && !(mods & Qt::AltModifier)) {
        auto* tb = qobject_cast<FloatingToolbar*>(m_toolbar);
        auto applyTool = [this, tb](AnnotationTool t) {
            if (tb) tb->setActiveTool(t);
            m_overlay->setTool(t);
        };
        switch (key) {
        case Qt::Key_P: applyTool(AnnotationTool::Pen);         return;
        case Qt::Key_E: applyTool(AnnotationTool::Eraser);      return;
        case Qt::Key_T: applyTool(AnnotationTool::Text);        return;
        case Qt::Key_L: applyTool(AnnotationTool::Line);        return;
        case Qt::Key_R: applyTool(AnnotationTool::Rect);        return;
        case Qt::Key_O: applyTool(AnnotationTool::Ellipse);     return;
        case Qt::Key_A: applyTool(AnnotationTool::Arrow);       return;
        case Qt::Key_H: applyTool(AnnotationTool::Highlighter); return;
        case Qt::Key_K: applyTool(AnnotationTool::Laser);       return;
        case Qt::Key_C: m_overlay->clearAll();                  return;
        default: break;
        }
    }
    QWidget::keyPressEvent(event);
}

void AnnotationWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_overlay)
        m_overlay->setGeometry(rect());
}

void AnnotationWindow::closeEvent(QCloseEvent* event)
{
    emitClosedOnce();
    QWidget::closeEvent(event);
}

void AnnotationWindow::emitClosedOnce()
{
    if (m_closedEmitted) return;
    m_closedEmitted = true;
    emit closed();
}

// ──────────────────────────────────────────────
// dev-win compatibility (used by src/app/mainwindow_*.cpp)
// ──────────────────────────────────────────────

AnnotationWindow::AnnotationWindow(QWidget* parent, const QRect& targetRect)
    : AnnotationWindow(parent)
{
    if (targetRect.isValid())
        setTargetGeometry(targetRect);
}

QPixmap AnnotationWindow::annotationPixmap() const
{
    if (!m_overlay)
        return {};
    return QPixmap::fromImage(m_overlay->renderAnnotationsToImage(m_overlay->size()));
}

void AnnotationWindow::copyToClipboard()
{
    // Capture the screen behind our annotation layer, then overlay the
    // annotations themselves on top, then push the result to the clipboard.
    // The window is briefly hidden so the floating toolbar does not end up
    // baked into the screenshot.
    const QRect rect = geometry();
    if (!rect.isValid())
        return;

    QScreen* screen = QGuiApplication::screenAt(rect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const bool wasVisible = isVisible();
    if (wasVisible) {
        hide();
        // Give the OS one tick to actually repaint without our window.
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 80);
    }

    QPixmap fullScreen = screen->grabWindow(0);
    QPixmap base;
    if (!fullScreen.isNull()) {
        // grabWindow returns the entire screen; crop to our global geometry,
        // translated into screen-local coordinates.
        QRect local(rect.topLeft() - screen->geometry().topLeft(), rect.size());
        local = local.intersected(QRect(QPoint(0, 0), fullScreen.size()));
        base = fullScreen.copy(local);
    }

    if (wasVisible) {
        show();
        raise();
        activateWindow();
    }

    if (base.isNull())
        return;

    // Composite annotations on top.
    if (m_overlay && m_overlay->hasRenderableContent()) {
        QPixmap ink = annotationPixmap();
        if (!ink.isNull()) {
            if (ink.size() != base.size())
                ink = ink.scaled(base.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            QPainter painter(&base);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.drawPixmap(0, 0, ink);
        }
    }

    QApplication::clipboard()->setPixmap(base);

    if (m_overlay)
        m_overlay->showToast(QStringLiteral("✓ 已复制到剪贴板"));
}

#include "annotationwindow.moc"
