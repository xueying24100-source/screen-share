#include <QtTest/QtTest>

#include "media/capture/screen/screencapturer.h"

#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <QWidget>

class CaptureSmokeWindowsTest : public QObject
{
    Q_OBJECT

private slots:
    void capturesWindowAndScreen();
};

void CaptureSmokeWindowsTest::capturesWindowAndScreen()
{
    QWidget widget;
    widget.setWindowTitle(QStringLiteral("[Test] Capture smoke window"));
    widget.resize(320, 240);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget, 3000));

    QThread captureThread;
    ScreenCapturer capturer;
    capturer.moveToThread(&captureThread);
    captureThread.start();

    struct FrameStats {
        int totalFrames{0};
        int nonBlackFrames{0};
    } stats;

    QMetaObject::Connection frameConnection = connect(
        &capturer,
        &ScreenCapturer::frameCaptured,
        this,
        [&capturer, &stats](const QImage& frame) {
            ++stats.totalFrames;
            if (!ScreenCapturer::imageLooksMostlyBlack(frame)) {
                ++stats.nonBlackFrames;
            }
            capturer.releaseFrameSlot();
        },
        Qt::QueuedConnection);

    auto waitForFrames = [this, &capturer, &stats](auto starter, int minimumFrames, int timeoutMs) {
        stats = {};

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

        QMetaObject::Connection quitConnection = connect(
            &capturer,
            &ScreenCapturer::frameCaptured,
            this,
            [&loop, &stats, minimumFrames](const QImage&) {
                if (stats.totalFrames >= minimumFrames && loop.isRunning()) {
                    loop.quit();
                }
            },
            Qt::QueuedConnection);

        starter();
        timeoutTimer.start(timeoutMs);
        loop.exec();
        disconnect(quitConnection);
    };

    waitForFrames(
        [&capturer, &widget]() {
            QMetaObject::invokeMethod(
                &capturer,
                [&capturer, hwnd = static_cast<quintptr>(widget.winId())]() {
                    capturer.startWindow(hwnd, 30);
                },
                Qt::QueuedConnection);
        },
        5,
        3000);

    QVERIFY2(stats.totalFrames >= 5, "Window capture did not deliver at least five frames within 3 seconds");
    QVERIFY2(stats.nonBlackFrames >= 3, "Window capture frames were unexpectedly black");

    QMetaObject::invokeMethod(&capturer, [&capturer]() { capturer.stop(); }, Qt::BlockingQueuedConnection);
    QTest::qWait(200);
    const int framesAfterWindowStop = stats.totalFrames;
    QTest::qWait(400);
    QCOMPARE(stats.totalFrames, framesAfterWindowStop);

    waitForFrames(
        [&capturer]() {
            QMetaObject::invokeMethod(&capturer, [&capturer]() { capturer.startScreen(0, 30); }, Qt::QueuedConnection);
        },
        5,
        3000);

    QVERIFY2(stats.totalFrames >= 5, "Screen capture did not deliver at least five frames within 3 seconds");

    QMetaObject::invokeMethod(&capturer, [&capturer]() { capturer.stop(); }, Qt::BlockingQueuedConnection);
    QTest::qWait(200);
    const int framesAfterScreenStop = stats.totalFrames;
    QTest::qWait(400);
    QCOMPARE(stats.totalFrames, framesAfterScreenStop);

    disconnect(frameConnection);
    captureThread.quit();
    QVERIFY(captureThread.wait(3000));
}

QTEST_MAIN(CaptureSmokeWindowsTest)

#include "test_capture_smoke_windows.moc"
