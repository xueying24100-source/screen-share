#include "ScreenView.h"

#include <QPaintEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>

ScreenView::ScreenView(QWidget *parent)
    : QWidget(parent)
    , m_placeholderText("等待屏幕共享...")
{
    setMinimumSize(640, 480);
    setMouseTracking(true);
}

void ScreenView::setPlaceholderText(const QString &text)
{
    m_placeholderText = text;
    if (m_currentFrame.isNull()) {
        update();
    }
}

void ScreenView::updateFrame(const QImage &frame)
{
    m_currentFrame = frame;
    update();
}

void ScreenView::clearFrame()
{
    m_currentFrame = QImage();
    m_placeholderText = "等待屏幕共享...";
    update();
}

void ScreenView::setAnnotationTool(AnnotationTool tool)
{
    m_tool = tool;
}

void ScreenView::setAnnotationColor(const QColor &color)
{
    m_strokeColor = color;
}

void ScreenView::setAnnotationWidth(int width)
{
    m_strokeWidth = width;
}

void ScreenView::addAnnotation(const AnnotationCommand &command)
{
    m_annotations.append(command);
    update();
}

void ScreenView::clearAnnotations()
{
    m_annotations.clear();
    update();
}

void ScreenView::setAnnotationEnabled(bool enabled)
{
    m_annotationEnabled = enabled;
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

QRectF ScreenView::renderedContentRect() const
{
    if (m_currentFrame.isNull()) {
        return QRectF(rect());
    }
    QSizeF scaled = m_currentFrame.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    qreal x = (width() - scaled.width()) / 2.0;
    qreal y = (height() - scaled.height()) / 2.0;
    return QRectF(x, y, scaled.width(), scaled.height());
}

AnnotationPoint ScreenView::normalizedPoint(const QPointF &widgetPoint,
                                             const QRectF &contentRect) const
{
    if (contentRect.width() <= 0.0 || contentRect.height() <= 0.0 ||
        !contentRect.contains(widgetPoint)) {
        return AnnotationPoint{-1.0f, -1.0f};
    }
    return AnnotationPoint{
        static_cast<float>((widgetPoint.x() - contentRect.left()) / contentRect.width()),
        static_cast<float>((widgetPoint.y() - contentRect.top()) / contentRect.height())};
}

QPointF ScreenView::denormalizedPoint(const AnnotationPoint &point,
                                       const QRectF &contentRect) const
{
    return QPointF(contentRect.left() + point.x * contentRect.width(),
                   contentRect.top() + point.y * contentRect.height());
}

void ScreenView::drawAnnotation(QPainter *painter, const QRectF &contentRect,
                                 const AnnotationCommand &cmd) const
{
    if (cmd.tool != AnnotationTool::Pen && cmd.tool != AnnotationTool::Rectangle) {
        return;
    }

    QPen pen(cmd.style.color, cmd.style.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);

    if (cmd.tool == AnnotationTool::Pen) {
        if (cmd.points.size() == 1) {
            painter->drawPoint(denormalizedPoint(cmd.points.constFirst(), contentRect));
            return;
        }
        for (int i = 1; i < cmd.points.size(); ++i) {
            painter->drawLine(denormalizedPoint(cmd.points.at(i - 1), contentRect),
                              denormalizedPoint(cmd.points.at(i), contentRect));
        }
        return;
    }

    if (cmd.tool == AnnotationTool::Rectangle) {
        const QRectF rect(
            QPointF(contentRect.left() + cmd.normalizedRect.left() * contentRect.width(),
                    contentRect.top() + cmd.normalizedRect.top() * contentRect.height()),
            QPointF(contentRect.left() + cmd.normalizedRect.right() * contentRect.width(),
                    contentRect.top() + cmd.normalizedRect.bottom() * contentRect.height()));
        painter->drawRect(rect.normalized());
    }
}

void ScreenView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF contentRect = renderedContentRect();

    if (!m_currentFrame.isNull()) {
        QPixmap scaled = QPixmap::fromImage(m_currentFrame).scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.fillRect(rect(), QColor("#0a0a14"));
        painter.drawPixmap(x, y, scaled);
    } else {
        painter.fillRect(rect(), QColor("#1a1a2e"));
        painter.setPen(QColor("#666666"));
        painter.setFont(QFont("Microsoft YaHei", 14));
        painter.drawText(rect(), Qt::AlignCenter, m_placeholderText);
        return;
    }

    // 渲染已提交的标注
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto &cmd : m_annotations) {
        drawAnnotation(&painter, contentRect, cmd);
    }

    // 渲染正在绘制的标注
    if (m_drawing) {
        drawAnnotation(&painter, contentRect, m_currentCommand);
    }
}

void ScreenView::mousePressEvent(QMouseEvent *event)
{
    if (!m_annotationEnabled || event->button() != Qt::LeftButton || m_currentFrame.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF contentRect = renderedContentRect();
    const AnnotationPoint point = normalizedPoint(event->position(), contentRect);
    if (!isValidAnnotationPoint(point)) {
        return;
    }

    m_drawing = true;
    m_currentCommand = AnnotationCommand();
    m_currentCommand.commandId = QString("annotation-%1").arg(++m_commandSeq);
    m_currentCommand.objectId = m_currentCommand.commandId;
    m_currentCommand.userId = "local";
    m_currentCommand.tool = m_tool;
    m_currentCommand.action = AnnotationAction::Begin;
    m_currentCommand.style.color = m_strokeColor;
    m_currentCommand.style.width = static_cast<float>(m_strokeWidth);
    m_currentCommand.sourceCanvasSize = m_currentFrame.size();
    m_currentCommand.timestampMs = QDateTime::currentMSecsSinceEpoch();

    if (m_tool == AnnotationTool::Pen) {
        m_currentCommand.points.append(point);
    } else if (m_tool == AnnotationTool::Rectangle) {
        m_currentCommand.points.append(point);
        const QPointF qp = qPointFFromAnnotationPoint(point);
        m_currentCommand.normalizedRect = QRectF(qp, qp);
    }
    update();
}

void ScreenView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QRectF contentRect = renderedContentRect();
    const AnnotationPoint point = normalizedPoint(event->position(), contentRect);
    if (!isValidAnnotationPoint(point)) {
        return;
    }

    m_currentCommand.action = AnnotationAction::Update;

    if (m_currentCommand.tool == AnnotationTool::Pen) {
        if (m_currentCommand.points.isEmpty() ||
            m_currentCommand.points.constLast().x != point.x ||
            m_currentCommand.points.constLast().y != point.y) {
            m_currentCommand.points.append(point);
        }
    } else if (m_currentCommand.tool == AnnotationTool::Rectangle && !m_currentCommand.points.isEmpty()) {
        const QPointF startPt = qPointFFromAnnotationPoint(m_currentCommand.points.constFirst());
        m_currentCommand.normalizedRect = QRectF(startPt, qPointFFromAnnotationPoint(point)).normalized();
    }
    update();
}

void ScreenView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_drawing || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_currentCommand.action = AnnotationAction::Commit;
    m_currentCommand.timestampMs = QDateTime::currentMSecsSinceEpoch();

    bool valid = false;
    if (m_currentCommand.tool == AnnotationTool::Pen && !m_currentCommand.points.isEmpty()) {
        valid = true;
    } else if (m_currentCommand.tool == AnnotationTool::Rectangle &&
               m_currentCommand.normalizedRect.width() > 0.002 &&
               m_currentCommand.normalizedRect.height() > 0.002) {
        valid = true;
    }

    if (valid) {
        m_annotations.append(m_currentCommand);
        emit annotationCreated(m_currentCommand);
    }

    m_drawing = false;
    m_currentCommand = AnnotationCommand();
    update();
}
