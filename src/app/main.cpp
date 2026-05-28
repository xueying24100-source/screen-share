#include <QApplication>
#include "../ui/picker/windowpicker.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WindowPicker w;
    w.show();
    return a.exec();
}