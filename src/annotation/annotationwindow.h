#pragma once
#include <QWidget>
#include <QRect>
#include "annotationoverlay.h"

class QKeyEvent;
class QResizeEvent;
class QPixmap;

class AnnotationWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationWindow(QWidget* parent = nullptr, const QRect& targetGlobalRect = QRect());

    QPixmap annotationPixmap() const;
    bool hasAnnotationContent() const;
    void setAnnotationGeometry(const QRect& targetGlobalRect);

signals:
    void closed();
    void strokePacketReady(const StrokePacket& pkt);
    void textAnnotationCreated(const TextAnnotation& text);
    void contentChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    AnnotationOverlay* m_overlay = nullptr;
    QWidget*           m_toolbar = nullptr;
};
