#include <QApplication>
#include <QDebug>
#include "annotationwindow.h"
#include "audiocapturer.h"
#include "screencapturer.h"
#include "sender.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    DebugTransport transport;
    Sender sender;
    sender.setTransport(&transport);
    sender.start();

    ScreenCapturer screenCapturer;
    AudioCapturer audioCapturer;

    AnnotationWindow w;

    QObject::connect(&screenCapturer, &ScreenCapturer::frameCaptured,
                     &sender, &Sender::onMainScreenFrameCaptured);
    QObject::connect(&audioCapturer, &AudioCapturer::audioDataReady,
                     &sender, &Sender::onAudioDataReady);
    QObject::connect(&w, &AnnotationWindow::strokePacketReady,
                     &sender, &Sender::onStrokePacketReady);
    QObject::connect(&w, &AnnotationWindow::textAnnotationCreated,
                     &sender, &Sender::onTextAnnotationCreated);

    QObject::connect(&screenCapturer, &ScreenCapturer::captureError,
                     [](const QString& error) { qWarning() << "[ScreenCapturer]" << error; });
    QObject::connect(&audioCapturer, &AudioCapturer::captureError,
                     [](const QString& error) { qWarning() << "[AudioCapturer]" << error; });

    screenCapturer.start(15);
    audioCapturer.start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        audioCapturer.stop();
        screenCapturer.stop();
        sender.stop();
    });

    w.show();
    return app.exec();
}