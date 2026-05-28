#include <QtTest/QtTest>

#include "ui/annotation/annotationoverlay.h"

namespace {

Stroke makeStroke(std::initializer_list<QPointF> points)
{
    Stroke stroke;
    stroke.color = Qt::red;
    stroke.width = 3.0f;
    stroke.isEraser = false;
    for (const QPointF& point : points) {
        stroke.points.append(point);
    }
    return stroke;
}

} // namespace

class AnnotationOverlayTest : public QObject
{
    Q_OBJECT

private slots:
    void undoRedoKeepsExpectedStrokeCount();
    void renderProducesExpectedImage();
    void renderableContentReflectsState();
};

void AnnotationOverlayTest::undoRedoKeepsExpectedStrokeCount()
{
    AnnotationOverlay overlay;
    overlay.resize(320, 180);

    overlay.addStroke(makeStroke({QPointF(10, 10), QPointF(40, 40)}));
    overlay.addStroke(makeStroke({QPointF(20, 20), QPointF(50, 50)}));
    overlay.addStroke(makeStroke({QPointF(30, 30), QPointF(60, 60)}));

    QCOMPARE(overlay.strokes().size(), 3);

    overlay.undo();
    overlay.undo();
    overlay.redo();

    QCOMPARE(overlay.strokes().size(), 2);
}

void AnnotationOverlayTest::renderProducesExpectedImage()
{
    AnnotationOverlay overlay;
    overlay.resize(320, 180);
    overlay.addStroke(makeStroke({QPointF(10, 10), QPointF(40, 40), QPointF(80, 30)}));

    const QSize targetSize(640, 360);
    const QImage image = overlay.renderAnnotationsToImage(targetSize);

    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), targetSize);
}

void AnnotationOverlayTest::renderableContentReflectsState()
{
    AnnotationOverlay overlay;
    overlay.resize(320, 180);

    QVERIFY(!overlay.hasRenderableContent());

    overlay.addStroke(makeStroke({QPointF(12, 24), QPointF(64, 96)}));

    QVERIFY(overlay.hasRenderableContent());
}

QTEST_MAIN(AnnotationOverlayTest)

#include "test_annotation_overlay.moc"
