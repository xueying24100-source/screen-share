#ifndef SCREENVIEW_H
#define SCREENVIEW_H

#include <QWidget>
#include <QImage>
#include <QVector>

#include "AnnotationTypes.h"

class ScreenView : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenView(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);
    void updateFrame(const QImage &frame);
    void clearFrame();

    void setAnnotationTool(AnnotationTool tool);
    void setAnnotationColor(const QColor &color);
    void setAnnotationWidth(int width);
    void addAnnotation(const AnnotationCommand &command);
    void clearAnnotations();
    void setAnnotationEnabled(bool enabled);

signals:
    void annotationCreated(const AnnotationCommand &command);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF renderedContentRect() const;
    AnnotationPoint normalizedPoint(const QPointF &widgetPoint, const QRectF &contentRect) const;
    QPointF denormalizedPoint(const AnnotationPoint &point, const QRectF &contentRect) const;
    void drawAnnotation(QPainter *painter, const QRectF &contentRect,
                        const AnnotationCommand &cmd) const;

    QString m_placeholderText;
    QImage m_currentFrame;

    QVector<AnnotationCommand> m_annotations;
    AnnotationCommand m_currentCommand;
    bool m_drawing = false;
    bool m_annotationEnabled = false;
    AnnotationTool m_tool = AnnotationTool::Pen;
    QColor m_strokeColor = QColor("#ff3b30");
    int m_strokeWidth = 3;
    int m_commandSeq = 0;
};

#endif // SCREENVIEW_H
