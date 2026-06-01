#ifndef SOURCEENUMERATOR_H
#define SOURCEENUMERATOR_H

#include <QList>
#include <QRect>
#include <QSize>
#include <QString>
#include <QtGlobal>

struct ScreenInfo {
    int index = 0;
    QString name;
    QSize resolution;
    QRect geometry;
};

struct WindowInfo {
    quintptr handle = 0;
    QString title;
    bool minimized = false;
};

class SourceEnumerator
{
public:
    static QList<ScreenInfo> enumerateScreens();
    static QList<WindowInfo> enumerateWindows();
};

#endif // SOURCEENUMERATOR_H

