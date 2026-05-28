QT += widgets

TEMPLATE = app
CONFIG += app_bundle c++17 objective_c++
TARGET = screen_capture
QMAKE_TARGET_BUNDLE_PREFIX = com.yuzhuo
QMAKE_INFO_PLIST = Info.plist
QMAKE_POST_LINK += /bin/cp $$PWD/Info.plist $$OUT_PWD/$${TARGET}.app/Contents/Info.plist
INCLUDEPATH += $$PWD/include

SOURCES += \
    src/demo/main.cpp \
    src/macos/ScreenCaptureManager.mm

HEADERS += \
    include/screen_share/AnnotationTypes.h \
    include/screen_share/ScreenCaptureManager.h

LIBS += -framework CoreGraphics \
        -framework CoreVideo \
        -framework CoreMedia \
        -framework CoreFoundation \
        -framework IOSurface \
        -framework ScreenCaptureKit
