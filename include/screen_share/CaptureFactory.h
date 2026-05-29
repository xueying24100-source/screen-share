#pragma once

#include <QObject>

namespace ss {

class ICapturer;
class ISourceEnumerator;

ICapturer* createCapturer(QObject* parent = nullptr);
ISourceEnumerator* createSourceEnumerator();

} // namespace ss
