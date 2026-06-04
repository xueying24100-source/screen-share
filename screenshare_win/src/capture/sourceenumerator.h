#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

struct WindowInfo {
    quintptr handle;
    QString  title;
    bool     minimized;
};

class SourceEnumerator {
public:
    static QList<WindowInfo> enumerateWindows();
};
