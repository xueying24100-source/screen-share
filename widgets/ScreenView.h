#ifndef SCREENVIEW_H
#define SCREENVIEW_H

#include <QWidget>
#include <QTimer>
#include <QImage>

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

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onSimulateTick();

private:
    void drawSimulatedFrame(QPainter &painter);

    QString m_placeholderText;
    bool m_showingContent = false;
    QImage m_currentFrame;

    // 模拟画面相关
    QTimer *m_simulateTimer = nullptr;
    QString m_sharerName;
    int m_tickCount = 0;
};

#endif // SCREENVIEW_H
