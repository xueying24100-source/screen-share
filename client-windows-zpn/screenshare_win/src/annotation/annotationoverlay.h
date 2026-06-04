#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QMap>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <QTimer>
#include <QWidget>

// ──────────────────────────────────────────────
// Enums
// ──────────────────────────────────────────────

enum class StrokeType : quint8 {
    FreePen     = 0,
    Line        = 1,
    Rect        = 2,
    Ellipse     = 3,
    Arrow       = 4,
    Highlighter = 5
};

enum class AnnotationTool {
    Pen,
    Eraser,
    Text,
    Line,
    Rect,
    Ellipse,
    Arrow,
    Highlighter,
    Laser
};

enum class StrokeEventType : quint8 {
    Begin     = 0,
    Point     = 1,
    End       = 2,
    Undo      = 3,
    Clear     = 4
};

// ──────────────────────────────────────────────
// Data structures (value semantics, POD-like)
// ──────────────────────────────────────────────

struct Stroke {
    StrokeType     type          = StrokeType::FreePen;
    QList<QPointF> points;
    QList<float>   pointWidths;  // optional; if size == points.size(), used for variable-width rendering
    QColor         color         = Qt::red;
    float          width         = 3.0f;
    bool           isEraser      = false;
    bool           isHighlighter = false;
    quint32        userId        = 0;
    quint32        strokeId      = 0;
};

struct TextAnnotation {
    QPointF position;
    QString text;
    QColor  color    = Qt::red;
    int     fontSize = 16;
    quint32 userId   = 0;
};

struct StrokePacket {
    StrokeEventType type          = StrokeEventType::Begin;
    quint32         userId        = 0;
    quint32         strokeId      = 0;
    QPointF         point;
    QColor          color         = Qt::red;
    float           width         = 3.0f;
    StrokeType      strokeType    = StrokeType::FreePen;
    bool            isEraser      = false;
    bool            isHighlighter = false;
};

// ──────────────────────────────────────────────
// AnnotationOverlay
// ──────────────────────────────────────────────

class QLineEdit;

class AnnotationOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationOverlay(quint32 userId, QWidget* parent = nullptr);

    bool canUndo() const { return !m_undoHistory.isEmpty(); }
    bool canRedo() const { return !m_redoHistory.isEmpty(); }
    bool isEditingText() const { return m_textEditor != nullptr; }
    AnnotationTool currentTool() const { return m_currentTool; }
    bool hasRenderableContent() const;

    QList<Stroke> strokes() const { return m_strokes; }
    QImage renderAnnotationsToImage(const QSize& targetSize) const;

public slots:
    void setPenColor(const QColor& color);
    void setPenWidth(float width);
    void setTool(AnnotationTool tool);
    void setFontSize(int pointSize);
    void setToolbarExcludeRect(const QRect& r);
    void clearAll();
    void undo();
    void redo();
    void addStroke(const Stroke& stroke);
    void applyRemotePacket(const StrokePacket& pkt);
    void commitTextInput();
    void cancelTextInput();
    void showToast(const QString& text, int durationMs = 1800);

    // Stress test: generates N random strokes, measures wall-clock time
    // to render them all, and surfaces the result via showToast() +
    // qDebug() output. Used by F12 in debug runs.
    void runStressTest(int strokeCount = 1000);

signals:
    void strokePacketReady(const StrokePacket& pkt);
    void strokeFinished(const Stroke& stroke);
    void undoRedoChanged();
    void textAnnotationCreated(const TextAnnotation& text);
    void toolChanged(AnnotationTool tool);
    void contentChanged();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    static QPainterPath buildSmoothPath(const QList<QPointF>& pts);
    void renderStroke(QPainter& painter, const Stroke& stroke) const;
    void renderFreePen(QPainter& painter, const Stroke& stroke) const;
    void renderLine(QPainter& painter, const Stroke& stroke) const;
    void renderRect(QPainter& painter, const Stroke& stroke) const;
    void renderEllipse(QPainter& painter, const Stroke& stroke) const;
    void renderArrow(QPainter& painter, const Stroke& stroke) const;
    void renderHighlighter(QPainter& painter, const Stroke& stroke) const;
    void drawTextAnnotation(QPainter& painter, const TextAnnotation& item) const;
    void paintAnnotations(QPainter& painter) const;
    void renderStrokeToCache(const Stroke& stroke);
    void rebuildCache();

    void beginTextInput(const QPointF& pos);
    StrokeType strokeTypeForCurrentTool() const;
    bool isShapeTool() const;
    bool shouldShowCursorPreview() const;
    QPointF applyShiftConstraint(const QPointF& start, const QPointF& current) const;

    void tickLaser();

    // Strokes
    QPixmap        m_cachedPixmap;
    QList<Stroke>  m_strokes;
    Stroke         m_currentStroke;
    bool           m_drawing       = false;
    QColor         m_penColor      = Qt::red;
    float          m_penWidth      = 3.0f;
    QRect          m_toolbarExcludeRect;
    quint32        m_strokeIdCounter = 0;
    quint32        m_userId        = 0;
    QMap<quint64, Stroke> m_remoteStrokes;   // key = (userId << 32) | strokeId

    // Text annotations
    QList<TextAnnotation> m_textAnnotations;
    QLineEdit*            m_textEditor = nullptr;
    QPointF               m_pendingTextPos;
    int                   m_fontSize   = 16;

    // Current tool
    AnnotationTool m_currentTool = AnnotationTool::Pen;

    // Shape preview
    bool           m_shiftHeld   = false;

    // Velocity-based pen pressure simulation
    QElapsedTimer  m_velocityClock;
    qint64         m_lastPointTimeNs = 0;
    float          m_smoothedWidth   = 3.0f;

    // Cursor preview circle (shows current pen width for drawing tools)
    QPointF        m_cursorPos;
    bool           m_cursorInside = false;

    // Transient toast text (bottom-right, fades after timer)
    QString        m_toastText;
    QTimer         m_toastTimer;

    // Laser pointer state
    QPointF        m_laserPos;
    qreal          m_laserAlpha  = 0.0;
    QTimer         m_laserTimer;

    // Undo/redo
    enum class ActionType { Stroke, Text };
    struct UndoAction {
        ActionType     type;
        Stroke         stroke;
        TextAnnotation text;
    };
    QList<UndoAction> m_undoHistory;
    QList<UndoAction> m_redoHistory;
};
