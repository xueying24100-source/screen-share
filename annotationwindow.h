#pragma once
#include <QWidget>
#include "annotationoverlay.h"

class QKeyEvent;

class AnnotationWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationWindow(QWidget* parent = nullptr);

signals:
    void closed();
    void strokePacketReady(const StrokePacket& pkt);
    void textAnnotationCreated(const TextAnnotation& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    AnnotationOverlay* m_overlay = nullptr;
    QWidget*           m_toolbar = nullptr;
};
