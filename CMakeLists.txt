cmake_minimum_required(VERSION 3.19)
project(screenShare LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(screenShare
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    pages/LoginPage.cpp
    pages/LoginPage.h
    pages/RoomPage.cpp
    pages/RoomPage.h
    widgets/ScreenView.cpp
    widgets/ScreenView.h
    widgets/ToolButton.cpp
    widgets/ToolButton.h
    widgets/MemberList.cpp
    widgets/MemberList.h
    network/RoomServer.cpp
    network/RoomServer.h
    network/RoomClient.cpp
    network/RoomClient.h
    capture/screencapturer.h
    capture/sourceenumerator.cpp
    capture/sourceenumerator.h
    capture/sharesourcepicker.cpp
    capture/sharesourcepicker.h
    capture/AnnotationTypes.h
)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(screenShare PRIVATE capture/screencapturer.mm)
    target_link_libraries(screenShare PRIVATE
        "-framework ApplicationServices"
        "-framework CoreVideo"
        "-framework ScreenCaptureKit"
    )
else()
    target_sources(screenShare PRIVATE capture/screencapturer.cpp)
endif()

qt_add_translations(
    TARGETS screenShare
    TS_FILES screenShare_zh_CN.ts
)

target_include_directories(screenShare PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pages
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/capture
)

target_link_libraries(screenShare
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

install(TARGETS screenShare
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET screenShare
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
