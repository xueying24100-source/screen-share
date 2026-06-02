#pragma once

#include <QList>
#include <QRect>
#include <QSize>
#include <QString>
#include <QtGlobal>

struct ScreenInfo {
    int     index;
    QString name;
    QSize   resolution;
    QRect   geometry;
};

struct WindowInfo {
    quintptr handle;
    QString  title;
    bool     minimized;
};

class SourceEnumerator {
public:
    static QList<ScreenInfo> enumerateScreens();
    static QList<WindowInfo> enumerateWindows();
};
