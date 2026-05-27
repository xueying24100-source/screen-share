#include <QApplication>
#include "wgctestwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    WgcTestWindow w;
    w.setWindowTitle("WGC Window Capture Test");
    w.resize(1000, 640);
    w.show();
    return app.exec();
}
