#pragma once
#include <QWidget>
#include "annotationoverlay.h"

class QCloseEvent;
class QKeyEvent;
class QResizeEvent;

class AnnotationWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationWindow(QWidget* parent = nullptr);
    void setTargetGeometry(const QRect& screenRect);
    void requestExit();
    QImage renderAnnotationsToImage(const QSize& targetSize) const;

signals:
    void closed();
    void strokePacketReady(const StrokePacket& pkt);
    void textAnnotationCreated(const TextAnnotation& text);
    void contentChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void emitClosedOnce();

    AnnotationOverlay* m_overlay = nullptr;
    QWidget*           m_toolbar = nullptr;
    bool               m_closedEmitted = false;
};
