cmake_minimum_required(VERSION 3.19)
project(qtproject_screenshare LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Multimedia Network)

qt_standard_project_setup()

qt_add_executable(qtproject_screenshare
    WIN32 MACOSX_BUNDLE
    # Entry point
    src/main.cpp

    # App layer - UI windows
    src/app/mainwindow.h src/app/mainwindow.cpp src/app/mainwindow.ui
    src/app/loginwindow.h src/app/loginwindow.cpp src/app/loginwindow.ui
    src/app/senderwindow.h src/app/senderwindow.cpp src/app/senderwindow.ui
    src/app/receiverwindow.h src/app/receiverwindow.cpp src/app/receiverwindow.ui
    src/app/sharetoolbar.h src/app/sharetoolbar.cpp

    # Media layer - video/audio processing
    src/media/sender.h src/media/sender.cpp
    src/media/mediareceiver.h src/media/mediareceiver.cpp
    src/media/videoencodeworker.h src/media/videoencodeworker.cpp
    src/media/videodecodeworker.h src/media/videodecodeworker.cpp
    src/media/cameramanager.h src/media/cameramanager.cpp

    # Audio layer - audio capture and playback
    src/audio/audiocapturer.h src/audio/audiocapturer.cpp
    src/audio/systemaudiocapturer.h src/audio/systemaudiocapturer.cpp
    src/audio/audiomixer.h src/audio/audiomixer.cpp
    src/audio/audioplayer.h src/audio/audioplayer.cpp

    # Annotation layer - drawing and markup
    src/annotation/annotationoverlay.h src/annotation/annotationoverlay.cpp
    src/annotation/annotationwindow.h src/annotation/annotationwindow.cpp

    # Network layer - transport
    src/network/networktransport.h src/network/networktransport.cpp

    # Capture layer - screen/window enumeration and capture
    src/capture/sourceenumerator.h src/capture/sourceenumerator.cpp
    src/capture/screencapturer.h src/capture/screencapturer.cpp

    # Platform layer - Windows-specific backends
    src/platform/windows/wgc/wgcwindowcapturebackend.h src/platform/windows/wgc/wgcwindowcapturebackend.cpp
)

# Translation resource generation is disabled for the classroom demo.
# It is not required for runtime, and MSVC/Qt Creator can fail on the generated
# qrc_qtproject_screenshare_translations.cpp file in some Chinese/Unicode paths.
# qtproject_screenshare_zh_CN.ts is kept in the source tree for future use.

target_link_libraries(qtproject_screenshare
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Multimedia
        Qt::Network
)

if(WIN32)
    target_compile_definitions(qtproject_screenshare PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
    target_compile_options(qtproject_screenshare PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
        $<$<CXX_COMPILER_ID:MSVC>:/await:strict>
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    )
    target_link_libraries(qtproject_screenshare
        PRIVATE
            user32
            ole32
            mmdevapi
            avrt
            d3d11
            dxgi
            windowsapp
            propsys
    )
endif()

install(TARGETS qtproject_screenshare
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET qtproject_screenshare
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
