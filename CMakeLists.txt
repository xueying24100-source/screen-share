cmake_minimum_required(VERSION 3.19)
project(screenShare LANGUAGES CXX OBJCXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Multimedia LinguistTools)

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
    include/screen_share/AnnotationTypes.h
    include/screen_share/ScreenCaptureManager.h
    src/macos/ScreenCaptureManager.mm
)

qt_add_translations(
    TARGETS screenShare
    TS_FILES screenShare_zh_CN.ts
)

target_include_directories(screenShare PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pages
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(screenShare
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Multimedia
)

if(APPLE)
    set_target_properties(screenShare PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )

    target_link_libraries(screenShare
        PRIVATE
            "-framework CoreGraphics"
            "-framework CoreVideo"
            "-framework CoreMedia"
            "-framework CoreFoundation"
            "-framework IOSurface"
            "-framework ScreenCaptureKit"
    )
endif()

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
