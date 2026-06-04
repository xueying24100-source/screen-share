#include "annotationoverlay.h"
#include "annotationwindow.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QtMath>
#include <QtGlobal>

namespace {

constexpr int   kLaserInnerRadius = 7;
constexpr int   kLaserOuterRadius = 16;
constexpr qreal kLaserFadeStep    = 0.025;   // slower fade ≈ 1.2 s tail
constexpr int   kLaserTimerMs     = 30;
constexpr int   kArrowMinHeadSize = 15;

quint64 packStrokeKey(quint32 userId, quint32 strokeId)
{
    return (static_cast<quint64>(userId) << 32) | strokeId;
}

} // namespace

// ──────────────────────────────────────────────
// Smooth path: Catmull-Rom -> Cubic Bezier (ADR-005)
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
        const QPointF p0 = pts[qMax(i - 1, 0)];
        const QPointF p1 = pts[i];
        const QPointF p2 = pts[i + 1];
        const QPointF p3 = pts[qMin(i + 2, pts.size() - 1)];
        const QPointF ctrl1 = p1 + (p2 - p0) / 6.0;
        const QPointF ctrl2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(ctrl1, ctrl2, p2);
    }
    return path;
}

// ──────────────────────────────────────────────
// Per-type renderers (ADR-004 dispatch)
// ──────────────────────────────────────────────

void AnnotationOverlay::renderStroke(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.points.isEmpty())
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    if (stroke.isHighlighter) {
        renderHighlighter(painter, stroke);
        return;
    }

    if (stroke.isEraser) {
        // Eraser: only ever applies to FreePen-style strokes
        renderFreePen(painter, stroke);
        return;
    }

    switch (stroke.type) {
    case StrokeType::FreePen:     renderFreePen(painter, stroke);     break;
    case StrokeType::Line:        renderLine(painter, stroke);        break;
    case StrokeType::Rect:        renderRect(painter, stroke);        break;
    case StrokeType::Ellipse:     renderEllipse(painter, stroke);     break;
    case StrokeType::Arrow:       renderArrow(painter, stroke);       break;
    case StrokeType::Highlighter: renderHighlighter(painter, stroke); break;
    }
}

void AnnotationOverlay::renderFreePen(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.isEraser) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        QPen pen(Qt::transparent, stroke.width * 3,
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(buildSmoothPath(stroke.points));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        return;
    }

    // Variable-width rendering when pressure data is available.
    // We sacrifice the global Bezier smoothing for per-segment width control,
    // but use rounded caps and joins to keep the visual smooth-ish.
    if (stroke.pointWidths.size() == stroke.points.size()
        && stroke.points.size() >= 2) {
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < stroke.points.size() - 1; ++i) {
            const float w = qMax(0.8f,
                                 (stroke.pointWidths[i] + stroke.pointWidths[i + 1]) * 0.5f);
            QPen pen(stroke.color, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawLine(stroke.points[i], stroke.points[i + 1]);
        }
        return;
    }

    // Constant-width Bezier path (default rendering)
    QPen pen(stroke.color, stroke.width,
             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(buildSmoothPath(stroke.points));
}

void AnnotationOverlay::renderLine(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.points.size() < 2)
        return;
    QPen pen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(stroke.points.first(), stroke.points.last());
}

void AnnotationOverlay::renderRect(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.points.size() < 2)
        return;
    QPen pen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF r(stroke.points.first(), stroke.points.last());
    painter.drawRect(r.normalized());
}

void AnnotationOverlay::renderEllipse(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.points.size() < 2)
        return;
    QPen pen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF r(stroke.points.first(), stroke.points.last());
    painter.drawEllipse(r.normalized());
}

void AnnotationOverlay::renderArrow(QPainter& painter, const Stroke& stroke) const
{
    if (stroke.points.size() < 2)
        return;

    const QPointF start = stroke.points.first();
    const QPointF end   = stroke.points.last();
    const QLineF line(start, end);
    if (qFuzzyIsNull(line.length()))
        return;

    QPen pen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(stroke.color);

    // Arrow head size scales with line width but has a minimum
    const qreal headSize = qMax<qreal>(kArrowMinHeadSize, stroke.width * 4.0);

    // Compute arrow head triangle
    const qreal angle = std::atan2(line.dy(), line.dx());
    const qreal a1 = angle + M_PI - M_PI / 7.0;
    const qreal a2 = angle + M_PI + M_PI / 7.0;
    const QPointF p1 = end + QPointF(std::cos(a1) * headSize, std::sin(a1) * headSize);
    const QPointF p2 = end + QPointF(std::cos(a2) * headSize, std::sin(a2) * headSize);

    // Shorten the shaft so it doesn't poke through the head tip
    const qreal shorten = headSize * 0.6;
    const QPointF shaftEnd = end - QPointF(std::cos(angle) * shorten, std::sin(angle) * shorten);
    painter.drawLine(start, shaftEnd);

    QPainterPath head;
    head.moveTo(end);
    head.lineTo(p1);
    head.lineTo(p2);
    head.closeSubpath();
    painter.drawPath(head);
}

void AnnotationOverlay::renderHighlighter(QPainter& painter, const Stroke& stroke) const
{
    // Semi-transparent stroke; do NOT use CompositionMode_Multiply because the
    // overlay is fully transparent, and multiply against (0,0,0,0) yields black.
    // Plain alpha blending lets the OS-level window compositor show the
    // underlying screen through the translucent stroke instead.
    QColor c = stroke.color;
    c.setAlpha(80);
    QPen pen(c, qMax(stroke.width * 3.0f, 14.0f),
             Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(buildSmoothPath(stroke.points));
}

// ──────────────────────────────────────────────
// Cache management
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

void AnnotationOverlay::rebuildCache()
{
    m_cachedPixmap = QPixmap(size());
    m_cachedPixmap.fill(Qt::transparent);
    for (const Stroke& s : m_strokes)
        renderStrokeToCache(s);
}

// ──────────────────────────────────────────────
// Text annotation rendering (with subtle shadow)
// ──────────────────────────────────────────────

void AnnotationOverlay::drawTextAnnotation(QPainter& painter, const TextAnnotation& item) const
{
    QFont font = painter.font();
    font.setPointSize(item.fontSize);
    painter.setFont(font);

    const QFontMetrics fm(font);
    painter.setPen(QColor(0, 0, 0, 100));
    painter.drawText(QPointF(item.position.x() + 1, item.position.y() + fm.ascent() + 1), item.text);

    painter.setPen(item.color);
    painter.drawText(QPointF(item.position.x(), item.position.y() + fm.ascent()), item.text);
}

void AnnotationOverlay::paintAnnotations(QPainter& painter) const
{
    if (!m_cachedPixmap.isNull())
        painter.drawPixmap(0, 0, m_cachedPixmap);

    for (const Stroke& s : m_remoteStrokes)
        renderStroke(painter, s);

    if (m_drawing)
        renderStroke(painter, m_currentStroke);

    for (const TextAnnotation& ta : m_textAnnotations)
        drawTextAnnotation(painter, ta);

    // Laser pointer on top: soft outer glow + bright inner dot
    if (m_laserAlpha > 0.0) {
        painter.setPen(Qt::NoPen);

        // Outer glow
        QColor glow(255, 30, 30, static_cast<int>(m_laserAlpha * 90));
        painter.setBrush(glow);
        painter.drawEllipse(m_laserPos, kLaserOuterRadius, kLaserOuterRadius);

        // Inner core
        QColor core(255, 60, 60, static_cast<int>(m_laserAlpha * 235));
        painter.setBrush(core);
        painter.drawEllipse(m_laserPos, kLaserInnerRadius, kLaserInnerRadius);
    }
}

QImage AnnotationOverlay::renderAnnotationsToImage(const QSize& targetSize) const
{
    if (!targetSize.isValid() || width() <= 0 || height() <= 0)
        return {};

    QImage image(targetSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.scale(targetSize.width() / double(width()),
                      targetSize.height() / double(height()));
        paintAnnotations(painter);
    }
    return image;
}

// ──────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────

AnnotationOverlay::AnnotationOverlay(quint32 userId, QWidget* parent)
    : QWidget(parent)
    , m_userId(userId)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet("background: transparent;");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_laserTimer.setInterval(kLaserTimerMs);
    connect(&m_laserTimer, &QTimer::timeout, this, &AnnotationOverlay::tickLaser);

    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toastText.clear();
        update();
    });
}

bool AnnotationOverlay::hasRenderableContent() const
{
    return !m_strokes.isEmpty()
        || !m_remoteStrokes.isEmpty()
        || !m_textAnnotations.isEmpty()
        || m_drawing
        || m_laserAlpha > 0.0;
}

// ──────────────────────────────────────────────
// Public slots: tool & state setters
// ──────────────────────────────────────────────

void AnnotationOverlay::setPenColor(const QColor& color)
{
    m_penColor = color;
    // Selecting a color switches off eraser/text/laser back to pen
    if (m_currentTool == AnnotationTool::Eraser
        || m_currentTool == AnnotationTool::Text
        || m_currentTool == AnnotationTool::Laser) {
        m_currentTool = AnnotationTool::Pen;
        setCursor(Qt::ArrowCursor);
        emit toolChanged(m_currentTool);
    }
}

void AnnotationOverlay::setPenWidth(float width)
{
    m_penWidth = qMax(1.0f, width);
}

void AnnotationOverlay::setTool(AnnotationTool tool)
{
    if (m_currentTool == tool)
        return;

    // If a stroke is in progress, cancel it so the user does not get a
    // mid-stream tool switch (e.g. a free pen path that should have been a rect).
    if (m_drawing) {
        m_drawing = false;
        m_currentStroke.points.clear();
        update();
        emit contentChanged();
    }

    // Cancel any pending text input when switching away from text
    if (m_currentTool == AnnotationTool::Text && m_textEditor)
        cancelTextInput();

    m_currentTool = tool;
    setCursor(tool == AnnotationTool::Text ? Qt::IBeamCursor : Qt::ArrowCursor);

    // Stop laser timer when leaving laser mode
    if (tool != AnnotationTool::Laser) {
        m_laserTimer.stop();
        m_laserAlpha = 0.0;
    }
    emit toolChanged(m_currentTool);
    update();
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
    m_laserAlpha = 0.0;
    m_laserTimer.stop();
    if (!m_cachedPixmap.isNull())
        m_cachedPixmap.fill(Qt::transparent);

    update();
    emit undoRedoChanged();
    emit contentChanged();

    StrokePacket pkt;
    pkt.type   = StrokeEventType::Clear;
    pkt.userId = m_userId;
    emit strokePacketReady(pkt);
}

// ──────────────────────────────────────────────
// Undo / Redo (ADR-007: precise delete by userId+strokeId)
// ──────────────────────────────────────────────

void AnnotationOverlay::undo()
{
    if (m_undoHistory.isEmpty())
        return;

    UndoAction action = m_undoHistory.takeLast();
    m_redoHistory.append(action);

    if (action.type == ActionType::Stroke) {
        const quint32 uid = action.stroke.userId;
        const quint32 sid = action.stroke.strokeId;

        for (int i = m_strokes.size() - 1; i >= 0; --i) {
            if (m_strokes[i].userId == uid && m_strokes[i].strokeId == sid) {
                m_strokes.removeAt(i);
                break;
            }
        }
        rebuildCache();

        StrokePacket pkt;
        pkt.type     = StrokeEventType::Undo;
        pkt.userId   = uid;
        pkt.strokeId = sid;
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

// ──────────────────────────────────────────────
// Remote packet handling
// ──────────────────────────────────────────────

void AnnotationOverlay::applyRemotePacket(const StrokePacket& pkt)
{
    const quint64 key = packStrokeKey(pkt.userId, pkt.strokeId);

    switch (pkt.type) {
    case StrokeEventType::Begin: {
        Stroke s;
        s.type          = pkt.strokeType;
        s.color         = pkt.color;
        s.width         = pkt.width;
        s.isEraser      = pkt.isEraser;
        s.isHighlighter = pkt.isHighlighter;
        s.userId        = pkt.userId;
        s.strokeId      = pkt.strokeId;
        s.points.append(pkt.point);
        m_remoteStrokes[key] = s;
        update();
        break;
    }
    case StrokeEventType::Point: {
        auto it = m_remoteStrokes.find(key);
        if (it != m_remoteStrokes.end()) {
            Stroke& s = it.value();
            // For shape tools the remote keeps only [start, current] (2 points)
            if (s.type != StrokeType::FreePen && s.type != StrokeType::Highlighter) {
                if (s.points.size() == 1) {
                    s.points.append(pkt.point);
                } else {
                    s.points.last() = pkt.point;
                }
            } else {
                s.points.append(pkt.point);
            }
            update();
            emit contentChanged();
        }
        break;
    }
    case StrokeEventType::End: {
        auto it = m_remoteStrokes.find(key);
        Stroke s;
        if (it != m_remoteStrokes.end()) {
            s = it.value();
            m_remoteStrokes.erase(it);
        } else {
            // Recover from possibly lost Begin/Point packets using End's full attrs
            s.type          = pkt.strokeType;
            s.color         = pkt.color;
            s.width         = pkt.width;
            s.isEraser      = pkt.isEraser;
            s.isHighlighter = pkt.isHighlighter;
            s.userId        = pkt.userId;
            s.strokeId      = pkt.strokeId;
        }
        if (s.points.isEmpty() || s.points.last() != pkt.point)
            s.points.append(pkt.point);

        renderStrokeToCache(s);
        m_strokes.append(s);
        m_redoHistory.clear();
        update();
        emit undoRedoChanged();
        emit strokeFinished(s);
        emit contentChanged();
        break;
    }
    case StrokeEventType::Undo: {
        for (int i = m_strokes.size() - 1; i >= 0; --i) {
            if (m_strokes[i].userId == pkt.userId
                && m_strokes[i].strokeId == pkt.strokeId) {
                m_strokes.removeAt(i);
                rebuildCache();
                update();
                emit undoRedoChanged();
                emit contentChanged();
                break;
            }
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
    if (m_textEditor)
        commitTextInput();

    m_pendingTextPos = pos;
    m_textEditor = new QLineEdit(this);

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
        ta.userId   = m_userId;

        m_textAnnotations.append(ta);
        m_undoHistory.append({ActionType::Text, {}, ta});
        m_redoHistory.clear();

        emit textAnnotationCreated(ta);
        emit undoRedoChanged();
        update();
        emit contentChanged();
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
    emit contentChanged();
}

bool AnnotationOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_textEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                cancelTextInput();
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut) {
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
    paintAnnotations(painter);

    // Cursor preview circle — UI affordance for the local screen only.
    // Excluded from paintAnnotations() so it does NOT leak into the
    // shared-frame composite (renderAnnotationsToImage).
    if (m_cursorInside && shouldShowCursorPreview() && !m_drawing
        && !(m_toolbarExcludeRect.isValid()
             && m_toolbarExcludeRect.contains(m_cursorPos.toPoint()))) {
        const float w = qMax(2.0f, m_penWidth);
        const float r = (m_currentTool == AnnotationTool::Eraser) ? w * 1.5f
                                                                  : w * 0.5f + 1;
        QPen ringPen(QColor(0, 0, 0, 200), 1.5);
        painter.setPen(ringPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(m_cursorPos, r, r);
        ringPen.setColor(QColor(255, 255, 255, 200));
        painter.setPen(ringPen);
        painter.drawEllipse(m_cursorPos, r - 1.5, r - 1.5);
    }

    // Toast: transient feedback at bottom-right (e.g. "已复制到剪贴板")
    if (!m_toastText.isEmpty()) {
        QFont f = painter.font();
        f.setPointSize(13);
        f.setBold(true);
        painter.setFont(f);
        const QFontMetrics fm(f);
        const int textW = fm.horizontalAdvance(m_toastText);
        const int padX = 14, padY = 8;
        const int boxW = textW + padX * 2;
        const int boxH = fm.height() + padY * 2;
        const int x = width() - boxW - 24;
        const int y = height() - boxH - 24;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 30, 220));
        painter.drawRoundedRect(QRect(x, y, boxW, boxH), 8, 8);
        painter.setPen(QColor(255, 255, 255, 235));
        painter.drawText(QRect(x, y, boxW, boxH), Qt::AlignCenter, m_toastText);
    }
}

// ──────────────────────────────────────────────
// Tool helpers
// ──────────────────────────────────────────────

StrokeType AnnotationOverlay::strokeTypeForCurrentTool() const
{
    switch (m_currentTool) {
    case AnnotationTool::Line:        return StrokeType::Line;
    case AnnotationTool::Rect:        return StrokeType::Rect;
    case AnnotationTool::Ellipse:     return StrokeType::Ellipse;
    case AnnotationTool::Arrow:       return StrokeType::Arrow;
    case AnnotationTool::Highlighter: return StrokeType::Highlighter;
    default:                          return StrokeType::FreePen;
    }
}

bool AnnotationOverlay::isShapeTool() const
{
    return m_currentTool == AnnotationTool::Line
        || m_currentTool == AnnotationTool::Rect
        || m_currentTool == AnnotationTool::Ellipse
        || m_currentTool == AnnotationTool::Arrow;
}

QPointF AnnotationOverlay::applyShiftConstraint(const QPointF& start, const QPointF& current) const
{
    if (!m_shiftHeld)
        return current;

    const QPointF delta = current - start;
    const qreal absX = std::abs(delta.x());
    const qreal absY = std::abs(delta.y());

    if (m_currentTool == AnnotationTool::Line) {
        // Snap to horizontal / vertical / 45°
        const qreal angle = std::atan2(delta.y(), delta.x()) * 180.0 / M_PI;
        const qreal snapAngle = std::round(angle / 45.0) * 45.0;
        const qreal rad = snapAngle * M_PI / 180.0;
        const qreal length = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
        return start + QPointF(std::cos(rad) * length, std::sin(rad) * length);
    }
    if (m_currentTool == AnnotationTool::Rect
        || m_currentTool == AnnotationTool::Ellipse) {
        // Snap to square / circle
        const qreal side = qMax(absX, absY);
        return start + QPointF(delta.x() < 0 ? -side : side,
                               delta.y() < 0 ? -side : side);
    }
    return current;
}

// ──────────────────────────────────────────────
// Laser pointer
// ──────────────────────────────────────────────

void AnnotationOverlay::tickLaser()
{
    m_laserAlpha -= kLaserFadeStep;
    if (m_laserAlpha <= 0.0) {
        m_laserAlpha = 0.0;
        m_laserTimer.stop();
    }
    update();
    emit contentChanged();
}

void AnnotationOverlay::showToast(const QString& text, int durationMs)
{
    m_toastText = text;
    m_toastTimer.start(qMax(300, durationMs));
    update();
}

void AnnotationOverlay::runStressTest(int strokeCount)
{
    // Generate `strokeCount` random free-pen strokes without network emission.
    // Each stroke has 20 random points; we measure how long the full
    // renderStrokeToCache pass takes and log the result.
    static quint32 baseUserId = 0xDEAD0000;
    const QSizeF bounds(qMax(width(), 200), qMax(height(), 200));
    uint seed = 42;
    auto rng = [&seed, &bounds](int axis) -> qreal {
        seed = seed * 1664525u + 1013904223u;  // LCG
        return (seed & 0xFFFF) / 65535.0 * (axis == 0 ? bounds.width() : bounds.height());
    };

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < strokeCount; ++i) {
        Stroke s;
        s.type    = StrokeType::FreePen;
        s.color   = QColor::fromHsv((i * 37) % 360, 200, 200);
        s.width   = 2.0f + (i % 5);
        s.userId  = baseUserId;
        s.strokeId = static_cast<quint32>(i);

        const QPointF origin(rng(0), rng(1));
        s.points.append(origin);
        for (int j = 1; j < 20; ++j) {
            const qreal dx = (rng(0) - bounds.width()  * 0.5) * 0.08;
            const qreal dy = (rng(1) - bounds.height() * 0.5) * 0.08;
            s.points.append(s.points.last() + QPointF(dx, dy));
        }

        renderStrokeToCache(s);
        m_strokes.append(s);
    }

    const qint64 ms = timer.elapsed();
    update();

    const QString msg = QStringLiteral("压测 %1 笔 → %2 ms").arg(strokeCount).arg(ms);
    qDebug() << "[StressTest]" << msg
             << "  FPS(estimate) =" << (strokeCount > 0 ? qRound(strokeCount * 1000.0 / qMax<qint64>(1, ms)) : 0)
             << "  strokes/s";

    showToast(msg, 4000);
    emit undoRedoChanged();
    emit contentChanged();
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
    if (m_toolbarExcludeRect.isValid() && m_toolbarExcludeRect.contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (m_currentTool == AnnotationTool::Text) {
        beginTextInput(event->position());
        return;
    }

    if (m_currentTool == AnnotationTool::Laser) {
        // Laser is movement-driven; press just refreshes the position
        m_laserPos = event->position();
        m_laserAlpha = 1.0;
        m_laserTimer.start();
        update();
        emit contentChanged();
        return;
    }

    // ── Pen / Eraser / Highlighter / Shape tools ──
    m_redoHistory.clear();

    ++m_strokeIdCounter;
    m_currentStroke.points.clear();
    m_currentStroke.type          = strokeTypeForCurrentTool();
    m_currentStroke.color         = m_penColor;
    m_currentStroke.width         = m_penWidth;
    m_currentStroke.isEraser      = (m_currentTool == AnnotationTool::Eraser);
    m_currentStroke.isHighlighter = (m_currentTool == AnnotationTool::Highlighter);
    m_currentStroke.userId        = m_userId;
    m_currentStroke.strokeId      = m_strokeIdCounter;
    m_currentStroke.points.append(event->position());
    m_currentStroke.pointWidths.clear();
    // Seed pressure tracking. Only FreePen + Pen tool gets velocity-based width.
    if (m_currentStroke.type == StrokeType::FreePen
        && m_currentTool == AnnotationTool::Pen
        && !m_currentStroke.isEraser) {
        m_smoothedWidth = m_penWidth;
        m_currentStroke.pointWidths.append(m_smoothedWidth);
        m_velocityClock.restart();
        m_lastPointTimeNs = m_velocityClock.nsecsElapsed();
    }
    m_drawing = true;

    StrokePacket pkt;
    pkt.type          = StrokeEventType::Begin;
    pkt.userId        = m_userId;
    pkt.strokeId      = m_strokeIdCounter;
    pkt.point         = event->position();
    pkt.color         = m_penColor;
    pkt.width         = m_penWidth;
    pkt.strokeType    = m_currentStroke.type;
    pkt.isEraser      = m_currentStroke.isEraser;
    pkt.isHighlighter = m_currentStroke.isHighlighter;
    emit strokePacketReady(pkt);

    update();
    emit contentChanged();
}

void AnnotationOverlay::mouseMoveEvent(QMouseEvent* event)
{
    // Always track cursor position for the preview circle.
    const QPointF prevCursor = m_cursorPos;
    m_cursorPos = event->position();
    if (shouldShowCursorPreview() && m_cursorInside) {
        const float r = qMax(20.0f, m_penWidth) + 4;
        QRectF dirty = QRectF(prevCursor, m_cursorPos).normalized()
                           .adjusted(-r, -r, r, r);
        update(dirty.toRect());
    }

    if (m_currentTool == AnnotationTool::Laser) {
        m_laserPos = event->position();
        m_laserAlpha = 1.0;
        m_laserTimer.start();
        update();
        return;
    }

    if (!m_drawing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QPointF pos = event->position();

    // Shape tools keep only [start, current]
    if (isShapeTool()) {
        const QPointF start = m_currentStroke.points.first();
        pos = applyShiftConstraint(start, pos);
        if (m_currentStroke.points.size() == 1)
            m_currentStroke.points.append(pos);
        else
            m_currentStroke.points.last() = pos;

        StrokePacket pkt;
        pkt.type     = StrokeEventType::Point;
        pkt.userId   = m_userId;
        pkt.strokeId = m_strokeIdCounter;
        pkt.point    = pos;
        emit strokePacketReady(pkt);

        update();
        emit contentChanged();
        return;
    }

    // FreePen / Highlighter / Eraser: append every distinct point
    if (m_currentStroke.points.isEmpty() || m_currentStroke.points.last() != pos) {
        const QPointF prevPt = m_currentStroke.points.isEmpty()
                                   ? pos : m_currentStroke.points.last();
        m_currentStroke.points.append(pos);

        // Velocity-based pressure simulation (only for Pen tool on FreePen strokes).
        // Lower speed → wider line, higher speed → thinner. Smoothed with EMA so
        // the variation looks like a real pen rather than jittery noise.
        if (!m_currentStroke.pointWidths.isEmpty()) {
            const qint64 nowNs = m_velocityClock.nsecsElapsed();
            const qreal dtMs = qMax<qreal>(1.0, (nowNs - m_lastPointTimeNs) / 1.0e6);
            m_lastPointTimeNs = nowNs;
            const QPointF d = pos - prevPt;
            const qreal dist = std::sqrt(d.x() * d.x() + d.y() * d.y());
            const qreal speed = dist / dtMs;            // px / ms
            // Map speed [0, 3+ px/ms] → factor [1.6, 0.4]
            const qreal factor = qBound(0.4, 1.6 - speed * 0.4, 1.6);
            const float targetW = m_penWidth * static_cast<float>(factor);
            // Smooth: pull toward target by 35% each step
            m_smoothedWidth = m_smoothedWidth * 0.65f + targetW * 0.35f;
            m_currentStroke.pointWidths.append(m_smoothedWidth);
        }

        StrokePacket pkt;
        pkt.type     = StrokeEventType::Point;
        pkt.userId   = m_userId;
        pkt.strokeId = m_strokeIdCounter;
        pkt.point    = pos;
        emit strokePacketReady(pkt);

        if (m_currentStroke.points.size() >= 2) {
            const QPointF prev = m_currentStroke.points[m_currentStroke.points.size() - 2];
            const float hw = m_currentStroke.isEraser
                                 ? m_currentStroke.width * 3 / 2.0f + 2
                                 : m_currentStroke.width * 1.6f / 2.0f + 2;
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
    if (isShapeTool() && !m_currentStroke.points.isEmpty())
        pos = applyShiftConstraint(m_currentStroke.points.first(), pos);

    if (isShapeTool()) {
        if (m_currentStroke.points.size() == 1)
            m_currentStroke.points.append(pos);
        else
            m_currentStroke.points.last() = pos;
    } else {
        if (m_currentStroke.points.isEmpty() || m_currentStroke.points.last() != pos)
            m_currentStroke.points.append(pos);
    }

    if (!m_currentStroke.points.isEmpty()) {
        renderStrokeToCache(m_currentStroke);
        m_strokes.append(m_currentStroke);
        m_undoHistory.append({ActionType::Stroke, m_currentStroke, {}});
        emit strokeFinished(m_currentStroke);
        emit undoRedoChanged();
        emit contentChanged();

        StrokePacket pkt;
        pkt.type          = StrokeEventType::End;
        pkt.userId        = m_userId;
        pkt.strokeId      = m_strokeIdCounter;
        pkt.point         = pos;
        pkt.color         = m_currentStroke.color;
        pkt.width         = m_currentStroke.width;
        pkt.strokeType    = m_currentStroke.type;
        pkt.isEraser      = m_currentStroke.isEraser;
        pkt.isHighlighter = m_currentStroke.isHighlighter;
        emit strokePacketReady(pkt);
    }

    m_currentStroke.points.clear();
    m_drawing = false;
    update();
    emit contentChanged();
}

// ──────────────────────────────────────────────
// Resize & keyboard
// ──────────────────────────────────────────────

void AnnotationOverlay::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    rebuildCache();
}

void AnnotationOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Shift) {
        m_shiftHeld = true;
        // No update needed; effect kicks in on next mouse move
    }

    if (event->key() == Qt::Key_Escape) {
        // Priority: cancel text input > cancel in-progress stroke > exit annotation
        if (isEditingText()) {
            cancelTextInput();
            event->accept();
            return;
        }
        if (m_drawing) {
            m_drawing = false;
            m_currentStroke.points.clear();
            update();
            emit contentChanged();
            event->accept();
            return;
        }
        if (auto* annotationWindow = qobject_cast<AnnotationWindow*>(parentWidget())) {
            annotationWindow->requestExit();
        } else {
            emit closeRequested();
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AnnotationOverlay::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Shift)
        m_shiftHeld = false;
    QWidget::keyReleaseEvent(event);
}

void AnnotationOverlay::enterEvent(QEnterEvent* event)
{
    m_cursorInside = true;
    m_cursorPos = event->position();
    if (shouldShowCursorPreview())
        update();
}

void AnnotationOverlay::leaveEvent(QEvent*)
{
    if (m_cursorInside && shouldShowCursorPreview())
        update();
    m_cursorInside = false;
}

bool AnnotationOverlay::shouldShowCursorPreview() const
{
    switch (m_currentTool) {
    case AnnotationTool::Pen:
    case AnnotationTool::Eraser:
    case AnnotationTool::Highlighter:
    case AnnotationTool::Line:
    case AnnotationTool::Rect:
    case AnnotationTool::Ellipse:
    case AnnotationTool::Arrow:
        return true;
    case AnnotationTool::Text:
    case AnnotationTool::Laser:
        return false;
    }
    return false;
}
