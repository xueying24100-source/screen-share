cmake_minimum_required(VERSION 3.16)
project(ScreenShare_Capturer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

set(SOURCES
    main.cpp
    mainwindow.cpp
    meetingmainwindow.cpp
    localpreviewwindow.cpp
    annotationoverlay.cpp
    annotationwindow.cpp
    wgctestwindow.cpp
    screencapturer.cpp
    wgcwindowcapturebackend.cpp
    sharesourcepicker.cpp
    sharetoolbar.cpp
    sourceenumerator.cpp
    audiocapturer.cpp
    systemaudiocapturer.cpp
    audiomixer.cpp
    audioplayer.cpp
    sender.cpp
)

set(HEADERS
    mainwindow.h
    meetingmainwindow.h
    localpreviewwindow.h
    annotationoverlay.h
    annotationwindow.h
    wgctestwindow.h
    screencapturer.h
    wgcwindowcapturebackend.h
    sharesourcepicker.h
    sharetoolbar.h
    sourceenumerator.h
    audiocapturer.h
    systemaudiocapturer.h
    audiomixer.h
    audioplayer.h
    sender.h
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dxgi user32 windowsapp ole32 mmdevapi propsys avrt)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /await:strict
        /Zc:__cplusplus
    )
endif()
