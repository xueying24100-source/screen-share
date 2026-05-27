#include "annotationoverlay.h"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QtGlobal>

// ──────────────────────────────────────────────
// Helper: build a smooth cubic-Bezier path from points
// ──────────────────────────────────────────────

QPainterPath AnnotationOverlay::buildSmoothPath(const QList<QPointF>& pts)
{
    QPainterPath path;
    if (pts.isEmpty())
        return path;
    if (pts.size() == 1) {
        path.moveTo(pts[0]);
        path.lineTo(pts[0]);
        return path;
    }
    if (pts.size() == 2) {
        path.moveTo(pts[0]);
        path.lineTo(pts[1]);
        return path;
    }
    path.moveTo(pts[0]);
    for (int i = 0; i < pts.size() - 1; ++i) {
        QPointF p0 = pts[qMax(i - 1, 0)];
        QPointF p1 = pts[i];
        QPointF p2 = pts[i + 1];
        QPointF p3 = pts[qMin(i + 2, pts.size() - 1)];
        QPointF ctrl1 = p1 + (p2 - p0) / 6.0;
        QPointF ctrl2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(ctrl1, ctrl2, p2);
    }
    return path;
}

// ──────────────────────────────────────────────
// Render a stroke onto an existing QPainter
// ──────────────────────────────────────────────

void AnnotationOverlay::renderStroke(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.points.isEmpty())
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    if (stroke.isEraser) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        QPen pen(Qt::transparent,
                 stroke.width * 3,
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    } else {
        QPen pen(stroke.color, stroke.width,
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    }

    painter.setBrush(Qt::NoBrush);
    painter.drawPath(buildSmoothPath(stroke.points));

    // Reset composition mode in case eraser was used
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

// ──────────────────────────────────────────────
// Render a finished stroke into the offscreen cache
// ──────────────────────────────────────────────

void AnnotationOverlay::renderStrokeToCache(const Stroke& stroke)
{
    if (stroke.points.isEmpty())
        return;

    if (m_cachedPixmap.isNull() || m_cachedPixmap.size() != size()) {
        m_cachedPixmap = QPixmap(size());
        m_cachedPixmap.fill(Qt::transparent);
    }

    QPainter p(&m_cachedPixmap);
    renderStroke(p, stroke);
}

// ──────────────────────────────────────────────
// Rebuild cache from scratch (used after undo)
// ──────────────────────────────────────────────

void AnnotationOverlay::rebuildCache()
{
    m_cachedPixmap = QPixmap(size());
    m_cachedPixmap.fill(Qt::transparent);
    for (const Stroke& s : m_strokes)
        renderStrokeToCache(s);
}

// ──────────────────────────────────────────────
// Draw a single text annotation
// ──────────────────────────────────────────────

void AnnotationOverlay::drawTextAnnotation(QPainter& painter, const TextAnnotation& item) const
{
    QFont font = painter.font();
    font.setPointSize(item.fontSize);
    painter.setFont(font);

    QFontMetrics fm(font);

    // Subtle shadow for readability on transparent overlay
    painter.setPen(QColor(0, 0, 0, 100));
    painter.drawText(QPointF(item.position.x() + 1, item.position.y() + fm.ascent() + 1), item.text);

    // Actual text
    painter.setPen(item.color);
    painter.drawText(QPointF(item.position.x(), item.position.y() + fm.ascent()), item.text);
}

// ──────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────

AnnotationOverlay::AnnotationOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet("background: transparent;");
    setMouseTracking(true);
}


QPixmap AnnotationOverlay::toPixmap() const
{
    QPixmap pixmap(size());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (!m_cachedPixmap.isNull())
        painter.drawPixmap(0, 0, m_cachedPixmap);

    // Draw remote / in-progress strokes as well, so the shared preview updates live.
    for (const Stroke& s : m_remoteStrokes)
        renderStroke(painter, s);

    if (m_drawing)
        renderStroke(painter, m_currentStroke);

    for (const TextAnnotation& ta : m_textAnnotations)
        drawTextAnnotation(painter, ta);

    return pixmap;
}

bool AnnotationOverlay::hasContent() const
{
    return m_drawing || !m_strokes.isEmpty() || !m_remoteStrokes.isEmpty() || !m_textAnnotations.isEmpty();
}

// ──────────────────────────────────────────────
// Public slots
// ──────────────────────────────────────────────

void AnnotationOverlay::setPenColor(const QColor& color)
{
    m_penColor = color;
    // Switching color implies pen mode (not eraser, not text)
    m_isEraser = false;
    m_currentTool = AnnotationTool::Pen;
    setCursor(Qt::ArrowCursor);
    emit toolChanged(m_currentTool);
}

void AnnotationOverlay::setPenWidth(float width)
{
    m_penWidth = qMax(1.0f, width);
}

void AnnotationOverlay::setEraserMode(bool on)
{
    m_isEraser = on;
    m_currentTool = on ? AnnotationTool::Eraser : AnnotationTool::Pen;
    setCursor(Qt::ArrowCursor);
    emit toolChanged(m_currentTool);
}

void AnnotationOverlay::setTextMode(bool on)
{
    if (on) {
        m_currentTool = AnnotationTool::Text;
        m_isEraser = false;
        setCursor(Qt::IBeamCursor);
    } else {
        m_currentTool = AnnotationTool::Pen;
        setCursor(Qt::ArrowCursor);
    }
    emit toolChanged(m_currentTool);
}

void AnnotationOverlay::setFontSize(int pointSize)
{
    m_fontSize = qMax(8, pointSize);
}

void AnnotationOverlay::setToolbarExcludeRect(const QRect& r)
{
    m_toolbarExcludeRect = r;
}

void AnnotationOverlay::clearAll()
{
    // Cancel any pending text input first
    if (m_textEditor) {
        QLineEdit* ed = m_textEditor;
        m_textEditor = nullptr;
        ed->hide();
        ed->deleteLater();
    }

    m_strokes.clear();
    m_textAnnotations.clear();
    m_undoHistory.clear();
    m_redoHistory.clear();
    m_currentStroke.points.clear();
    m_drawing = false;
    if (!m_cachedPixmap.isNull())
        m_cachedPixmap.fill(Qt::transparent);
    update();
    emit undoRedoChanged();
    emit contentChanged();

    StrokePacket pkt;
    pkt.type = StrokeEventType::Clear;
    emit strokePacketReady(pkt);
}

void AnnotationOverlay::undo()
{
    if (m_undoHistory.isEmpty())
        return;

    UndoAction action = m_undoHistory.takeLast();
    m_redoHistory.append(action);

    if (action.type == ActionType::Stroke) {
        if (!m_strokes.isEmpty())
            m_strokes.removeLast();
        rebuildCache();

        StrokePacket pkt;
        pkt.type = StrokeEventType::Undo;
        emit strokePacketReady(pkt);
    } else {
        if (!m_textAnnotations.isEmpty())
            m_textAnnotations.removeLast();
    }

    update();
    emit undoRedoChanged();
    emit contentChanged();
}

void AnnotationOverlay::redo()
{
    if (m_redoHistory.isEmpty())
        return;

    UndoAction action = m_redoHistory.takeLast();
    m_undoHistory.append(action);

    if (action.type == ActionType::Stroke) {
        renderStrokeToCache(action.stroke);
        m_strokes.append(action.stroke);
    } else {
        m_textAnnotations.append(action.text);
    }

    update();
    emit undoRedoChanged();
    emit contentChanged();
}

void AnnotationOverlay::addStroke(const Stroke& stroke)
{
    if (stroke.points.isEmpty())
        return;
    renderStrokeToCache(stroke);
    m_strokes.append(stroke);
    m_redoHistory.clear();
    update();
    emit undoRedoChanged();
    emit contentChanged();
}

void AnnotationOverlay::applyRemotePacket(const StrokePacket& pkt)
{
    switch (pkt.type) {
    case StrokeEventType::Begin: {
        Stroke s;
        s.color    = pkt.color;
        s.width    = pkt.width;
        s.isEraser = pkt.isEraser;
        s.points.append(pkt.point);
        m_remoteStrokes[pkt.strokeId] = s;
        break;
    }
    case StrokeEventType::Point: {
        if (m_remoteStrokes.contains(pkt.strokeId)) {
            m_remoteStrokes[pkt.strokeId].points.append(pkt.point);
            update();
            emit contentChanged();
        }
        break;
    }
    case StrokeEventType::End: {
        if (m_remoteStrokes.contains(pkt.strokeId)) {
            Stroke s = m_remoteStrokes.take(pkt.strokeId);
            if (!pkt.point.isNull())
                s.points.append(pkt.point);
            renderStrokeToCache(s);
            m_strokes.append(s);
            m_redoHistory.clear();
            update();
            emit undoRedoChanged();
            emit strokeFinished(s);
            emit contentChanged();
        }
        break;
    }
    case StrokeEventType::Undo: {
        if (!m_strokes.isEmpty()) {
            m_strokes.removeLast();
            rebuildCache();
            update();
            emit undoRedoChanged();
            emit contentChanged();
        }
        break;
    }
    case StrokeEventType::Clear: {
        m_strokes.clear();
        m_textAnnotations.clear();
        m_undoHistory.clear();
        m_redoHistory.clear();
        m_remoteStrokes.clear();
        if (!m_cachedPixmap.isNull())
            m_cachedPixmap.fill(Qt::transparent);
        update();
        emit undoRedoChanged();
        emit contentChanged();
        break;
    }
    }
}

// ──────────────────────────────────────────────
// Text input
// ──────────────────────────────────────────────

void AnnotationOverlay::beginTextInput(const QPointF& pos)
{
    // Commit any existing input first
    if (m_textEditor)
        commitTextInput();

    m_pendingTextPos = pos;
    m_textEditor = new QLineEdit(this);

    // Style to match overlay aesthetics
    QFont font;
    font.setPointSize(m_fontSize);
    m_textEditor->setFont(font);

    m_textEditor->setStyleSheet(
        QString("QLineEdit { color: %1; background: rgba(0,0,0,80);"
                " border: 1px solid rgba(255,255,255,160);"
                " border-radius: 3px; padding: 1px 3px; }")
            .arg(m_penColor.name()));
    m_textEditor->setMinimumWidth(160);
    m_textEditor->setMaximumWidth(600);
    m_textEditor->move(pos.toPoint());
    m_textEditor->show();
    m_textEditor->setFocus();

    // Catch Esc, Enter, and focus-loss via event filter
    m_textEditor->installEventFilter(this);

    connect(m_textEditor, &QLineEdit::returnPressed,
            this, &AnnotationOverlay::commitTextInput);
}

void AnnotationOverlay::commitTextInput()
{
    if (!m_textEditor)
        return;

    QString text = m_textEditor->text().trimmed();
    QLineEdit* ed = m_textEditor;
    m_textEditor = nullptr;
    ed->hide();
    ed->deleteLater();

    if (!text.isEmpty()) {
        TextAnnotation ta;
        ta.position = m_pendingTextPos;
        ta.text     = text;
        ta.color    = m_penColor;
        ta.fontSize = m_fontSize;

        m_textAnnotations.append(ta);
        m_undoHistory.append({ActionType::Text, {}, ta});
        m_redoHistory.clear();

        emit textAnnotationCreated(ta);
        emit undoRedoChanged();
        emit contentChanged();
        update();
    }
}

void AnnotationOverlay::cancelTextInput()
{
    if (!m_textEditor)
        return;

    QLineEdit* ed = m_textEditor;
    m_textEditor = nullptr;
    ed->hide();
    ed->deleteLater();
    update();
}

// ──────────────────────────────────────────────
// Event filter (for the temporary QLineEdit)
// ──────────────────────────────────────────────

bool AnnotationOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_textEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                cancelTextInput();
                return true; // consume the event
            }
            // Return/Enter is handled by the returnPressed signal connection
        }
        if (event->type() == QEvent::FocusOut) {
            // Defer commit/cancel so we're outside the event delivery chain
            QMetaObject::invokeMethod(this, [this]() {
                if (m_textEditor) {
                    if (!m_textEditor->text().trimmed().isEmpty())
                        commitTextInput();
                    else
                        cancelTextInput();
                }
            }, Qt::QueuedConnection);
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ──────────────────────────────────────────────
// Paint
// ──────────────────────────────────────────────

void AnnotationOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), QColor(0, 0, 0, 1));

    if (!m_cachedPixmap.isNull())
        painter.drawPixmap(0, 0, m_cachedPixmap);

    // Draw in-progress remote strokes
    for (const Stroke& s : m_remoteStrokes)
        renderStroke(painter, s);

    if (m_drawing)
        renderStroke(painter, m_currentStroke);

    // Draw committed text annotations
    for (const TextAnnotation& ta : m_textAnnotations)
        drawTextAnnotation(painter, ta);
}

// ──────────────────────────────────────────────
// Mouse events
// ──────────────────────────────────────────────

void AnnotationOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Don't draw/type inside the toolbar area
    if (m_toolbarExcludeRect.isValid() && m_toolbarExcludeRect.contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    // ── Text mode ────────────────────────────────────────────────────────
    if (m_currentTool == AnnotationTool::Text) {
        beginTextInput(event->position());
        return;
    }

    // ── Pen / Eraser mode ────────────────────────────────────────────────
    // A new stroke clears the redo stack
    m_redoHistory.clear();

    m_currentStroke.points.clear();
    m_currentStroke.color    = m_penColor;
    m_currentStroke.width    = m_penWidth;
    m_currentStroke.isEraser = m_isEraser;
    m_currentStroke.points.append(event->position());
    m_drawing = true;

    ++m_strokeIdCounter;
    StrokePacket pkt;
    pkt.type     = StrokeEventType::Begin;
    pkt.strokeId = m_strokeIdCounter;
    pkt.point    = event->position();
    pkt.color    = m_penColor;
    pkt.width    = m_penWidth;
    pkt.isEraser = m_isEraser;
    emit strokePacketReady(pkt);
    emit contentChanged();

    update();
}

void AnnotationOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_drawing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QPointF pos = event->position();
    if (m_currentStroke.points.isEmpty() || m_currentStroke.points.last() != pos) {
        m_currentStroke.points.append(pos);

        StrokePacket pkt;
        pkt.type     = StrokeEventType::Point;
        pkt.strokeId = m_strokeIdCounter;
        pkt.point    = pos;
        emit strokePacketReady(pkt);

        // Only repaint the bounding rect of the last two points for performance
        if (m_currentStroke.points.size() >= 2) {
            QPointF prev = m_currentStroke.points[m_currentStroke.points.size() - 2];
            float hw = m_currentStroke.isEraser
                           ? m_currentStroke.width * 3 / 2.0f + 2
                           : m_currentStroke.width / 2.0f + 2;
            QRectF dirty = QRectF(prev, pos).normalized().adjusted(-hw, -hw, hw, hw);
            update(dirty.toRect());
        } else {
            update();
        }
        emit contentChanged();
    }
}

void AnnotationOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_drawing || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    QPointF pos = event->position();
    if (m_currentStroke.points.isEmpty() || m_currentStroke.points.last() != pos)
        m_currentStroke.points.append(pos);

    if (!m_currentStroke.points.isEmpty()) {
        renderStrokeToCache(m_currentStroke);
        m_strokes.append(m_currentStroke);
        m_undoHistory.append({ActionType::Stroke, m_currentStroke, {}});
        emit strokeFinished(m_currentStroke);
        emit undoRedoChanged();

        StrokePacket pkt;
        pkt.type     = StrokeEventType::End;
        pkt.strokeId = m_strokeIdCounter;
        pkt.point    = pos;
        pkt.color    = m_currentStroke.color;
        pkt.width    = m_currentStroke.width;
        pkt.isEraser = m_currentStroke.isEraser;
        emit strokePacketReady(pkt);
    }

    m_currentStroke.points.clear();
    m_drawing = false;
    emit contentChanged();
    update();
}

// ──────────────────────────────────────────────
// Resize: rebuild offscreen cache at new size
// ──────────────────────────────────────────────

void AnnotationOverlay::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    rebuildCache();
}

