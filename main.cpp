#include <QApplication>
#include "meetingmainwindow.h"

#ifdef _WIN32
#include <winrt/base.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    winrt::init_apartment(winrt::apartment_type::single_threaded);
#endif

    QApplication app(argc, argv);
    MeetingMainWindow w;
    w.setWindowTitle(QStringLiteral("会议主面板"));
    w.resize(560, 220);
    w.show();
    const int code = app.exec();

#ifdef _WIN32
    winrt::uninit_apartment();
#endif
    return code;
}
