#pragma once
#include <QWidget>
#include <QColor>
#include <QPointF>
#include <QList>
#include <QPixmap>
#include <QMap>
#include <QRect>

// ──────────────────────────────────────────────
// Data structures
// ──────────────────────────────────────────────

struct Stroke {
    QList<QPointF> points;
    QColor         color;
    float          width    = 3.0f;
    bool           isEraser = false;
};

enum class StrokeEventType { Begin, Point, End, Undo, Clear };

struct StrokePacket {
    StrokeEventType type;
    quint32         strokeId = 0;
    QPointF         point;
    QColor          color;
    float           width    = 3.0f;
    bool            isEraser = false;
};

struct TextAnnotation {
    QPointF position;   // top-left anchor (in overlay coordinates)
    QString text;
    QColor  color    = Qt::red;
    int     fontSize = 16;  // point size
};

enum class AnnotationTool { Pen, Eraser, Text };

// ──────────────────────────────────────────────
// AnnotationOverlay
// ──────────────────────────────────────────────

class QLineEdit;

class AnnotationOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationOverlay(QWidget* parent = nullptr);

    bool canUndo() const { return !m_undoHistory.isEmpty(); }
    bool canRedo() const { return !m_redoHistory.isEmpty(); }
    bool isEditingText() const { return m_textEditor != nullptr; }
    AnnotationTool currentTool() const { return m_currentTool; }

    QList<Stroke> strokes() const { return m_strokes; }

public slots:
    void setPenColor(const QColor& color);
    void setPenWidth(float width);
    void setEraserMode(bool on);
    void setTextMode(bool on);
    void setFontSize(int pointSize);
    void setToolbarExcludeRect(const QRect& r);
    void clearAll();
    void undo();
    void redo();
    void addStroke(const Stroke& stroke);
    void applyRemotePacket(const StrokePacket& pkt);
    void commitTextInput();
    void cancelTextInput();

signals:
    void strokePacketReady(const StrokePacket& pkt);
    void strokeFinished(const Stroke& stroke);
    void undoRedoChanged();
    void textAnnotationCreated(const TextAnnotation& text);
    void toolChanged(AnnotationTool tool);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    static QPainterPath buildSmoothPath(const QList<QPointF>& pts);
    void renderStroke(QPainter& painter, const Stroke& stroke);
    void renderStrokeToCache(const Stroke& stroke);
    void rebuildCache();
    void beginTextInput(const QPointF& pos);
    void drawTextAnnotation(QPainter& painter, const TextAnnotation& item);

    // Strokes
    QPixmap        m_cachedPixmap;
    QList<Stroke>  m_strokes;
    Stroke         m_currentStroke;
    bool           m_drawing = false;
    QColor         m_penColor = Qt::red;
    float          m_penWidth = 3.0f;
    bool           m_isEraser = false;
    QRect          m_toolbarExcludeRect;
    quint32        m_strokeIdCounter = 0;
    QMap<quint32, Stroke> m_remoteStrokes;

    // Text annotations
    QList<TextAnnotation> m_textAnnotations;
    QLineEdit*            m_textEditor     = nullptr;
    QPointF               m_pendingTextPos;
    int                   m_fontSize       = 16;

    // Tool mode
    AnnotationTool m_currentTool = AnnotationTool::Pen;

    // Unified undo/redo (local operations only)
    enum class ActionType { Stroke, Text };
    struct UndoAction {
        ActionType     type;
        Stroke         stroke;
        TextAnnotation text;
    };
    QList<UndoAction> m_undoHistory;
    QList<UndoAction> m_redoHistory;
};
