cmake_minimum_required(VERSION 3.19)
project(qtproject_screenshare LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

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
)

qt_add_translations(
    TARGETS qtproject_screenshare
    TS_FILES qtproject_screenshare_zh_CN.ts
)

target_link_libraries(qtproject_screenshare
    PRIVATE
        Qt::Core
        Qt::Widgets
)

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
