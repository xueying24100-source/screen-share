cmake_minimum_required(VERSION 3.19)
project(qtproject_screenshare LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Multimedia Network)

qt_standard_project_setup()

qt_add_executable(qtproject_screenshare
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    loginwindow.h loginwindow.cpp loginwindow.ui
    senderwindow.h senderwindow.cpp senderwindow.ui
    receiverwindow.h receiverwindow.cpp receiverwindow.ui
    annotationoverlay.h annotationoverlay.cpp
    annotationwindow.h annotationwindow.cpp
    audiocapturer.h audiocapturer.cpp
    systemaudiocapturer.h systemaudiocapturer.cpp
    audiomixer.h audiomixer.cpp
    sender.h sender.cpp
    sharetoolbar.h sharetoolbar.cpp
    audioplayer.h audioplayer.cpp
    cameramanager.h cameramanager.cpp
    networktransport.h networktransport.cpp
    mediareceiver.h mediareceiver.cpp
    videoencodeworker.h videoencodeworker.cpp
    videodecodeworker.h videodecodeworker.cpp
)

# Translation resource generation is disabled for the classroom demo.
# It is not required for runtime, and MSVC/Qt Creator can fail on the generated
# qrc_qtproject_screenshare_translations.cpp file in some Chinese/Unicode paths.
# qtproject_screenshare_zh_CN.ts is kept in the source tree for future use.

target_compile_options(qtproject_screenshare PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

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
    target_link_libraries(qtproject_screenshare
        PRIVATE
            user32
            ole32
            mmdevapi
            avrt
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
