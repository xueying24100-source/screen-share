#include <QtTest/QtTest>

#include "media/capture/screen/screencapturer.h"

class ScreenCapturerHelpersTest : public QObject
{
    Q_OBJECT

private slots:
    void mostlyBlackImageIsDetected();
    void brightImageIsNotDetectedAsBlack();
    void thresholdBoundaryIsHandled();
};

void ScreenCapturerHelpersTest::mostlyBlackImageIsDetected()
{
    QImage image(QSize(64, 64), QImage::Format_RGB32);
    image.fill(Qt::black);

    QVERIFY(ScreenCapturer::imageLooksMostlyBlack(image));
}

void ScreenCapturerHelpersTest::brightImageIsNotDetectedAsBlack()
{
    QImage image(QSize(64, 64), QImage::Format_RGB32);
    image.fill(Qt::white);

    QVERIFY(!ScreenCapturer::imageLooksMostlyBlack(image));
}

void ScreenCapturerHelpersTest::thresholdBoundaryIsHandled()
{
    QImage darkGray(QSize(64, 64), QImage::Format_RGB32);
    darkGray.fill(QColor(29, 29, 29));
    QVERIFY(ScreenCapturer::imageLooksMostlyBlack(darkGray));

    QImage boundaryGray(QSize(64, 64), QImage::Format_RGB32);
    boundaryGray.fill(QColor(30, 30, 30));
    QVERIFY(!ScreenCapturer::imageLooksMostlyBlack(boundaryGray));

    QImage midGray(QSize(64, 64), QImage::Format_RGB32);
    midGray.fill(QColor(128, 128, 128));
    QVERIFY(!ScreenCapturer::imageLooksMostlyBlack(midGray));
}

QTEST_GUILESS_MAIN(ScreenCapturerHelpersTest)

#include "test_screen_capturer_helpers.moc"
