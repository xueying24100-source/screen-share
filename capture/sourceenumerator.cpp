#include "sourceenumerator.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#endif

QList<ScreenInfo> SourceEnumerator::enumerateScreens()
{
    QList<ScreenInfo> result;
    const QList<QScreen*> screens = QGuiApplication::screens();

    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens.at(i);
        if (!screen) {
            continue;
        }

        ScreenInfo info;
        info.index = i;
        info.name = QString("屏幕 %1").arg(i + 1);
        if (screen == QGuiApplication::primaryScreen()) {
            info.name += " (主屏)";
        }
        info.resolution = screen->size();
        info.geometry = screen->geometry();
        result.append(info);
    }

    return result;
}

QList<WindowInfo> SourceEnumerator::enumerateWindows()
{
    QList<WindowInfo> result;

#ifdef Q_OS_MACOS
    CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!windows) {
        return result;
    }

    const CFIndex count = CFArrayGetCount(windows);
    for (CFIndex i = 0; i < count; ++i) {
        auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));
        if (!dict) {
            continue;
        }

        int layer = 0;
        auto layerRef = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowLayer));
        if (!layerRef || !CFNumberGetValue(layerRef, kCFNumberIntType, &layer) || layer != 0) {
            continue;
        }

        CGWindowID windowId = 0;
        auto numberRef = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowNumber));
        if (!numberRef || !CFNumberGetValue(numberRef, kCFNumberIntType, &windowId)) {
            continue;
        }

        CGRect bounds = CGRectZero;
        auto boundsRef = static_cast<CFDictionaryRef>(CFDictionaryGetValue(dict, kCGWindowBounds));
        if (!boundsRef || !CGRectMakeWithDictionaryRepresentation(boundsRef, &bounds)) {
            continue;
        }
        if (bounds.size.width < 120 || bounds.size.height < 80) {
            continue;
        }

        QString owner;
        if (auto ownerRef = static_cast<CFStringRef>(CFDictionaryGetValue(dict, kCGWindowOwnerName))) {
            owner = QString::fromCFString(ownerRef).trimmed();
        }

        QString title;
        if (auto titleRef = static_cast<CFStringRef>(CFDictionaryGetValue(dict, kCGWindowName))) {
            title = QString::fromCFString(titleRef).trimmed();
        }

        if (owner.isEmpty() && title.isEmpty()) {
            continue;
        }

        WindowInfo info;
        info.handle = static_cast<quintptr>(windowId);
        info.title = title.isEmpty() ? owner : QString("%1 - %2").arg(owner, title);
        info.minimized = false;
        result.append(info);
    }

    CFRelease(windows);
#endif

    return result;
}

