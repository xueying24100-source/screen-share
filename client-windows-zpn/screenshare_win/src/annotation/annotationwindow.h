#pragma once
#include <QPixmap>
#include <QRect>
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

    // Convenience constructor used by MainWindow: build + set initial geometry.
    AnnotationWindow(QWidget* parent, const QRect& targetRect);

    void setTargetGeometry(const QRect& screenRect);
    void requestExit();
    QImage renderAnnotationsToImage(const QSize& targetSize) const;
    bool hasRenderableContent() const;
    void copyToClipboard();

    quint32 userId() const { return m_userId; }

    // ── dev-win compatibility shims (used by src/app/mainwindow_*.cpp) ──
    void setAnnotationGeometry(const QRect& r) { setTargetGeometry(r); }
    bool hasAnnotationContent() const { return hasRenderableContent(); }
    QPixmap annotationPixmap() const;

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
    static quint32 generateUserId();

    AnnotationOverlay* m_overlay = nullptr;
    QWidget*           m_toolbar = nullptr;
    bool               m_closedEmitted = false;
    quint32            m_userId = 0;
};
