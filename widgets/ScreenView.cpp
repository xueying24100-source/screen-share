#include "ScreenView.h"

#include <QPaintEvent>
#include <QPainter>
#include <QDateTime>
#include <QLinearGradient>
#include <QMouseEvent>

ScreenView::ScreenView(QWidget *parent)
    : QWidget(parent)
    , m_placeholderText("等待屏幕共享...")
{
    setMinimumSize(640, 480);

    m_simulateTimer = new QTimer(this);
    connect(m_simulateTimer, &QTimer::timeout, this, &ScreenView::onSimulateTick);
}

void ScreenView::setPlaceholderText(const QString &text)
{
    m_placeholderText = text;
    update();
}

void ScreenView::startSimulatedView(const QString &sharerName)
{
    m_sharerName = sharerName;
    m_tickCount = 0;
    m_showingContent = true;
    m_simulateTimer->start(500); // 每 500ms 刷新一次模拟画面
    update();
}

void ScreenView::stopSimulatedView()
{
    m_simulateTimer->stop();
    clearContent();
}

void ScreenView::updateFrame(const QImage &frame)
{
    m_currentFrame = frame;
    m_showingContent = true;
    update();
}

void ScreenView::clearContent(const QString &placeholderText)
{
    m_simulateTimer->stop();
    m_showingContent = false;
    m_currentFrame = QImage();
    m_annotationStrokes.clear();
    m_currentStroke.clear();
    m_drawingAnnotation = false;
    m_placeholderText = placeholderText;
    update();
}

void ScreenView::setAnnotationEnabled(bool enabled)
{
    m_annotationEnabled = enabled;
    if (!m_annotationEnabled) {
        m_drawingAnnotation = false;
        m_currentStroke.clear();
    }
    setCursor(m_annotationEnabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

bool ScreenView::annotationEnabled() const
{
    return m_annotationEnabled;
}

void ScreenView::clearAnnotations()
{
    m_annotationStrokes.clear();
    m_currentStroke.clear();
    m_drawingAnnotation = false;
    update();
}

void ScreenView::addRemoteStroke(const QVector<QPointF> &stroke)
{
    if (stroke.isEmpty()) {
        return;
    }

    m_annotationStrokes.append(stroke);
    update();
}

void ScreenView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRectF contentRect = renderedContentRect();

    if (m_showingContent && !m_currentFrame.isNull()) {
        // 真实帧渲染：等比缩放居中
        QPixmap scaled = QPixmap::fromImage(m_currentFrame).scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.fillRect(rect(), QColor("#0a0a14"));
        painter.drawPixmap(x, y, scaled);
        drawAnnotations(painter, contentRect);
        return;
    }

    if (m_showingContent) {
        // 模拟画面
        drawSimulatedFrame(painter);
        drawAnnotations(painter, contentRect);
        return;
    }

    // 占位画面
    painter.fillRect(rect(), QColor("#1a1a2e"));
    painter.setPen(QColor("#666666"));
    painter.setFont(QFont("Microsoft YaHei", 14));
    painter.drawText(rect(), Qt::AlignCenter, m_placeholderText);
}

void ScreenView::mousePressEvent(QMouseEvent *event)
{
    if (!m_annotationEnabled || !m_showingContent || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF contentRect = renderedContentRect();
    const QPointF point = normalizedPoint(event->position(), contentRect);
    if (point.x() < 0.0 || point.y() < 0.0) {
        return;
    }

    m_currentStroke.clear();
    m_currentStroke.append(point);
    m_drawingAnnotation = true;
    update();
}

void ScreenView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_annotationEnabled || !m_drawingAnnotation) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPointF point = normalizedPoint(event->position(), renderedContentRect());
    if (point.x() < 0.0 || point.y() < 0.0) {
        return;
    }

    if (m_currentStroke.isEmpty() || m_currentStroke.constLast() != point) {
        m_currentStroke.append(point);
        update();
    }
}

void ScreenView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_annotationEnabled || !m_drawingAnnotation || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (!m_currentStroke.isEmpty()) {
        m_annotationStrokes.append(m_currentStroke);
        emit annotationStrokeCommitted(m_currentStroke);
    }
    m_currentStroke.clear();
    m_drawingAnnotation = false;
    update();
}

void ScreenView::drawSimulatedFrame(QPainter &painter)
{
    // 背景 — 模拟桌面
    QLinearGradient bg(rect().topLeft(), rect().bottomRight());
    bg.setColorAt(0, "#1e3a5f");
    bg.setColorAt(1, "#0f1b2d");
    painter.fillRect(rect(), bg);

    int w = width();
    int h = height();

    // 模拟任务栏
    QRect taskbar(0, h - 36, w, 36);
    painter.fillRect(taskbar, QColor("#1a1a2e"));
    painter.setPen(QColor("#3a3a4a"));
    painter.drawLine(taskbar.topLeft(), taskbar.topRight());

    painter.setPen(Qt::white);
    painter.setFont(QFont("Microsoft YaHei", 9));
    painter.drawText(taskbar.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                     QDateTime::currentDateTime().toString("hh:mm"));

    // 模拟窗口
    int winW = qMin(w - 80, 700);
    int winH = qMin(h - 120, 440);
    int winX = (w - winW) / 2;
    int winY = (h - 36 - winH) / 2;
    QRect windowRect(winX, winY, winW, winH);

    // 窗口阴影
    painter.fillRect(windowRect.adjusted(4, 4, 4, 4), QColor(0, 0, 0, 60));
    // 窗口背景
    painter.fillRect(windowRect, QColor("#2b2b2b"));
    // 标题栏
    QRect titleBar(winX, winY, winW, 32);
    painter.fillRect(titleBar, QColor("#353535"));
    // 红绿黄三个圆点
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#ff5f57")); painter.drawEllipse(winX + 12, winY + 10, 12, 12);
    painter.setBrush(QColor("#febc2e")); painter.drawEllipse(winX + 30, winY + 10, 12, 12);
    painter.setBrush(QColor("#28c840")); painter.drawEllipse(winX + 48, winY + 10, 12, 12);
    // 标题文字
    painter.setPen(QColor("#aaaaaa"));
    painter.setFont(QFont("Microsoft YaHei", 9));
    painter.drawText(titleBar.adjusted(70, 0, 0, 0), Qt::AlignVCenter | Qt::AlignHCenter,
                     "屏幕共享演示");

    // 模拟内容区 — 显示共享者信息
    QRect contentRect(winX + 1, winY + 33, winW - 2, winH - 33);
    QLinearGradient contentBg(contentRect.topLeft(), contentRect.bottomRight());
    contentBg.setColorAt(0, "#2d2d3d");
    contentBg.setColorAt(1, "#1e1e2e");
    painter.fillRect(contentRect, contentBg);

    // 共享者名称
    painter.setPen(Qt::white);
    painter.setFont(QFont("Microsoft YaHei", 16, QFont::Bold));
    painter.drawText(contentRect.adjusted(0, -40, 0, 0),
                     Qt::AlignCenter, m_sharerName + " 的屏幕");

    // 动态帧计数器（模拟实时刷新）
    painter.setPen(QColor("#888888"));
    painter.setFont(QFont("Microsoft YaHei", 11));
    painter.drawText(contentRect.adjusted(0, 10, 0, 0),
                     Qt::AlignCenter,
                     QString("模拟帧 #%1  ·  %2")
                         .arg(m_tickCount)
                         .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    // 顶部状态标签
    QRect tagRect(w / 2 - 80, 8, 160, 28);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(45, 90, 160, 200));
    painter.drawRoundedRect(tagRect, 4, 4);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Microsoft YaHei", 10));
    painter.drawText(tagRect, Qt::AlignCenter, "● 正在观看");
}

void ScreenView::drawAnnotations(QPainter &painter, const QRectF &contentRect)
{
    if (contentRect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setClipRect(contentRect);
    QPen pen(QColor("#ff3b30"), 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);

    auto drawStroke = [&](const QVector<QPointF> &stroke) {
        if (stroke.size() == 1) {
            const QPointF p(contentRect.left() + stroke.constFirst().x() * contentRect.width(),
                            contentRect.top() + stroke.constFirst().y() * contentRect.height());
            painter.drawPoint(p);
            return;
        }
        for (int i = 1; i < stroke.size(); ++i) {
            const QPointF p1(contentRect.left() + stroke.at(i - 1).x() * contentRect.width(),
                             contentRect.top() + stroke.at(i - 1).y() * contentRect.height());
            const QPointF p2(contentRect.left() + stroke.at(i).x() * contentRect.width(),
                             contentRect.top() + stroke.at(i).y() * contentRect.height());
            painter.drawLine(p1, p2);
        }
    };

    for (const auto &stroke : m_annotationStrokes) {
        drawStroke(stroke);
    }
    if (m_drawingAnnotation) {
        drawStroke(m_currentStroke);
    }
    painter.restore();
}

QRectF ScreenView::renderedContentRect() const
{
    if (!m_showingContent || m_currentFrame.isNull()) {
        return QRectF(rect());
    }

    QSizeF scaled = m_currentFrame.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    const qreal x = (width() - scaled.width()) / 2.0;
    const qreal y = (height() - scaled.height()) / 2.0;
    return QRectF(x, y, scaled.width(), scaled.height());
}

QPointF ScreenView::normalizedPoint(const QPointF &widgetPoint, const QRectF &contentRect) const
{
    if (contentRect.width() <= 0.0 || contentRect.height() <= 0.0 ||
        !contentRect.contains(widgetPoint)) {
        return QPointF(-1.0, -1.0);
    }

    return QPointF((widgetPoint.x() - contentRect.left()) / contentRect.width(),
                   (widgetPoint.y() - contentRect.top()) / contentRect.height());
}

void ScreenView::onSimulateTick()
{
    m_tickCount++;
    update();
}
