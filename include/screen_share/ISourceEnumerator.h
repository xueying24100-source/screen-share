#pragma once

#include <QVector>

#include "screen_share/Types.h"

namespace ss {

class ISourceEnumerator {
public:
    virtual ~ISourceEnumerator() = default;
    virtual QVector<SourceInfo> listDisplays() = 0;
    virtual QVector<SourceInfo> listWindows() = 0;
};

} // namespace ss
