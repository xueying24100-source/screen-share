#include "media/capture/screen/sourceenumerator.h"

#include <QGuiApplication>
#include <QScreen>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

QList<ScreenInfo> SourceEnumerator::enumerateScreens()
{
    QList<ScreenInfo> screensInfo;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* screen = screens.at(i);
        if (!screen) {
            continue;
        }

        ScreenInfo info;
        info.index = i;
        info.name = QStringLiteral("屏幕 %1").arg(i + 1);
        if (i == 0) {
            info.name += QStringLiteral(" (主屏)");
        }
        info.resolution = screen->size();
        info.geometry = screen->geometry();
        screensInfo.append(info);
    }
    return screensInfo;
}

QList<WindowInfo> SourceEnumerator::enumerateWindows()
{
    QList<WindowInfo> windows;

#ifdef Q_OS_WIN
    struct EnumContext {
        QList<WindowInfo>* windows;
    } context{&windows};

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* context = reinterpret_cast<EnumContext*>(lParam);
            if (!context || !context->windows) {
                return TRUE;
            }

            if (!IsWindowVisible(hwnd)) {
                return TRUE;
            }
            if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
                return TRUE;
            }

            const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if (exStyle & WS_EX_TOOLWINDOW) {
                return TRUE;
            }

            const int titleLength = GetWindowTextLengthW(hwnd);
            if (titleLength <= 0) {
                return TRUE;
            }

            std::vector<wchar_t> titleBuffer(static_cast<size_t>(titleLength) + 1, L'\0');
            if (GetWindowTextW(hwnd, titleBuffer.data(), titleLength + 1) <= 0) {
                return TRUE;
            }
            const QString title = QString::fromWCharArray(titleBuffer.data());

            if (title.trimmed().isEmpty()) {
                return TRUE;
            }
            if (title == QStringLiteral("Program Manager") ||
                title == QStringLiteral("Windows 输入体验")) {
                return TRUE;
            }

            RECT rect{};
            if (!GetWindowRect(hwnd, &rect)) {
                return TRUE;
            }
            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;
            const bool minimized = IsIconic(hwnd);
            if (!minimized && (width < 120 || height < 80)) {
                return TRUE;
            }

            WindowInfo info;
            info.handle = reinterpret_cast<quintptr>(hwnd);
            info.title = title;
            info.minimized = minimized;
            context->windows->append(info);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&context));
#endif

    return windows;
}
