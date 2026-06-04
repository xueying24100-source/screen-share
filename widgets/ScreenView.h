#ifndef SCREENVIEW_H
#define SCREENVIEW_H

#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QPointF>
#include <QVector>

class ScreenView : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenView(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);

    // 模拟观看：显示一个模拟的共享桌面画面
    void startSimulatedView(const QString &sharerName);
    void stopSimulatedView();

    // 真实帧渲染接口（等采集模块就绪后使用）
    void updateFrame(const QImage &frame);
    void clearContent(const QString &placeholderText = QStringLiteral("等待屏幕共享..."));
    void setAnnotationEnabled(bool enabled);
    bool annotationEnabled() const;
    void clearAnnotations();
    void addRemoteStroke(const QVector<QPointF> &stroke);

signals:
    void annotationStrokeCommitted(const QVector<QPointF> &stroke);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onSimulateTick();

private:
    void drawSimulatedFrame(QPainter &painter);
    void drawAnnotations(QPainter &painter, const QRectF &contentRect);
    QRectF renderedContentRect() const;
    QPointF normalizedPoint(const QPointF &widgetPoint, const QRectF &contentRect) const;

    QString m_placeholderText;
    bool m_showingContent = false;
    QImage m_currentFrame;
    bool m_annotationEnabled = false;
    bool m_drawingAnnotation = false;
    QVector<QVector<QPointF>> m_annotationStrokes;
    QVector<QPointF> m_currentStroke;

    // 模拟画面相关
    QTimer *m_simulateTimer = nullptr;
    QString m_sharerName;
    int m_tickCount = 0;
};

#endif // SCREENVIEW_H
