#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 全局深色色调
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#1e1e1e"));
    palette.setColor(QPalette::WindowText, QColor("#cccccc"));
    palette.setColor(QPalette::Base, QColor("#2b2b2b"));
    palette.setColor(QPalette::AlternateBase, QColor("#353535"));
    palette.setColor(QPalette::Text, QColor("#cccccc"));
    palette.setColor(QPalette::Button, QColor("#3a3a3a"));
    palette.setColor(QPalette::ButtonText, QColor("#cccccc"));
    palette.setColor(QPalette::Highlight, QColor("#2d5aa0"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    a.setPalette(palette);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
